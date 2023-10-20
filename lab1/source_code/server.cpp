#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

using namespace std;
// 链接库
#pragma comment(lib,"ws2_32.lib")

#define THREAD_NUM 5
SOCKET ServerSocket = INVALID_SOCKET;  // 服务器套接字
SOCKADDR_IN ClientAddr = { 0 };  // 记录客户端地址，记录了其实也没啥用
int ClientAddrLen = sizeof(ClientAddr);

HANDLE HandleRecv[THREAD_NUM] = { NULL };  // 线程句柄
HANDLE mainHandle;  // 用于accept的线程句柄

time_t now_time;  // 用来打印时间
char now_time_str[20];

ofstream fout;


// 封装客户端信息结构体
struct Client
{
	SOCKET sClient;  // 客户端套接字
	char buffer[128];  // 数据缓冲区
	char userName[16];  // 客户端用户名
	char transID[16];  // 记录转发给谁
	UINT_PTR flag;  // 标记客户端，用来区分不同的客户端
};
Client Clients[THREAD_NUM] = { 0 };  // 客户端结构体,最多同时THREAD_NUM人在线


// 发送数据线程
unsigned __stdcall ThreadSend(void* param){
	int index = *(int*)param;

	//sprintf_s(Clients[index].buffer, "[%s]: %s", Clients[index].userName, Clients[index].buffer);  // 逐步替换，这样会错
	char temp[128] = { 0 };	 // 临时数据缓冲区，存放接收到的数据
	memcpy(temp, Clients[index].buffer, sizeof(temp));
	sprintf_s(Clients[index].buffer, "[%s]: %s", Clients[index].userName, temp);  // 格式化：把发送源的名字添加进信息里

	if (strlen(temp) != 0)  // 如果数据不为空且还没转发则转发
	{
		if (strcmp(Clients[index].transID, "all") == 0)  // 如果是给所有人转发
		{
			for (int j = 0; j < THREAD_NUM; j++) {
				// 需要确保当前client有效，否则发送失败了后续都不发了
				if (j != index && Clients[j].sClient != INVALID_SOCKET)  // 向除自己之外的所有客户端发送信息
				{
					if (send(Clients[j].sClient, Clients[index].buffer, sizeof(Clients[j].buffer), 0) == SOCKET_ERROR)
						return 1;
				}
			}
		}
		else
		{
			for (int j = 0; j < THREAD_NUM; j++)  // 向指定的人转发
				if (Clients[j].sClient != INVALID_SOCKET && j!=index && strcmp(Clients[j].userName, Clients[index].transID) == 0) // 可以重名了
				{
					if (send(Clients[j].sClient, Clients[index].buffer, sizeof(Clients[j].buffer), 0) == SOCKET_ERROR)
						return 1;
				}
		}
	}
	return 0;
}

// 接受数据线程
unsigned __stdcall ThreadRecv(void* param){
	SOCKET client = INVALID_SOCKET;
	int index = 0;
	for (int j = 0; j < THREAD_NUM; j++) 
		if (*(int*)param == Clients[j].flag)  // 判断是为哪个客户端开辟的线程
		{
			client = Clients[j].sClient;
			index = j;
			break;
		}

	char temp[128] = { 0 };  // 临时数据缓冲区
	while (true)
	{
		memset(temp, 0, sizeof(temp));
		// 接收数据
		if (recv(client, temp, sizeof(temp), 0) == SOCKET_ERROR)  // 忙等待
			continue;

		// 接收到数据，放进对应结构体的buffer
		memcpy(Clients[index].buffer, temp, sizeof(Clients[index].buffer));

		if (strcmp(temp, "exit") == 0)  // 如果客户发送exit请求，那么直接关闭线程，不打开转发线程
		{
			// 告诉其他用户该用户下线
			char tempMessage[100] = "[system]:";
			char sys[10] = "exit!";
			sprintf_s(tempMessage, "%s %s %s", tempMessage, Clients[index].userName, sys);
			for (int j = 0; j < THREAD_NUM; j++) {
				if (Clients[j].flag != 0 && j != index)
					send(Clients[j].sClient, tempMessage, sizeof(tempMessage), 0);
			}

			closesocket(Clients[index].sClient);  // 关闭该套接字
			CloseHandle(HandleRecv[index]);  // 关闭线程句柄
			Clients[index].sClient = INVALID_SOCKET;  // 位置空出来，留给以后进入的客户使用
			Clients[index].flag = 0;
			HandleRecv[index] = NULL;
			// 打印系统信息
			time(&now_time);
			strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
			cout <<"  "<< now_time_str << endl;  // 打印时间
			cout << "[system]: '" << Clients[index].userName << "' " << "has disconnected from the server. " << endl;
			fout << "  " << now_time_str << endl;  // 打印时间
			fout << "[system]: '" << Clients[index].userName << "' " << "has disconnected from the server. " << endl;
			break;
		}
		else
		{	// 输出到文件里
			time(&now_time);
			strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
			cout << "  " << now_time_str << endl;  // 打印时间
			cout << "[" << Clients[index].userName << "]: " << temp << " (to '" << Clients[index].transID << "')" << endl;
			fout << "  " << now_time_str << endl;  // 打印时间
			fout << "[" << Clients[index].userName << "]: " << temp << " (to '" << Clients[index].transID << "')" << endl;
			_beginthreadex(NULL, 0, ThreadSend, &index, 0, NULL);  // 开启一个转发线程,index标记着这个需要被转发的信息是从哪个线程来的；
		}
	}
	return 0;
}

// 接受请求
unsigned __stdcall ThreadAccept(void* param)
{
	int i = 0;
	while (true)
	{
		if (Clients[i].flag != 0)   // 寻找未连接的线程
		{
			i++;
			i %= THREAD_NUM;
			continue;
		}
		// 等待连接，如果有客户端申请连接就接受连接
		if ((Clients[i].sClient = accept(ServerSocket, (SOCKADDR*)&ClientAddr, &ClientAddrLen)) == INVALID_SOCKET)
		{
			cout << "Accept error: " << WSAGetLastError() << endl;
			closesocket(ServerSocket);
			WSACleanup();
			return -1;
		}

		// 告诉用户目前有哪些人
		char tempNameBuffer[300] = "At present these people are in the chat room: ";
		for (int j = 0; j < THREAD_NUM; j++) {
			if (Clients[j].flag != 0)
				sprintf_s(tempNameBuffer, "%s \n [%s]", tempNameBuffer, Clients[j].userName);  // 格式化：把发送源的名字添加进信息里
		}
		send(Clients[i].sClient, tempNameBuffer, sizeof(tempNameBuffer), 0);

		recv(Clients[i].sClient, Clients[i].userName, sizeof(Clients[i].userName), 0);  // 接收用户名

		//// 检查重名
		//bool flag = false;
		//char OK[3] = "ok", NO[3] = "no";
		//for (int j = 0; j < THREAD_NUM; j++) {
		//	if (Clients[j].flag != 0 && strcmp(Clients[j].userName, Clients[i].userName) == 0)
		//		flag = true;
		//}
		//if (flag) {  //重名了，往后稍稍
		//	send(Clients[i].sClient, NO, sizeof(NO), 0);
		//	continue;
		//}
		//else  // 发送确认信息
		//	send(Clients[i].sClient, OK, sizeof(OK), 0);

		recv(Clients[i].sClient, Clients[i].transID, sizeof(Clients[i].transID), 0);  // 接收转发的范围
		time(&now_time);
		strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
		// 打印系统提示
		cout << "  " << now_time_str << endl;  // 打印时间
		cout << "[system]: '" << Clients[i].userName << "'" << " connect successfully!" << endl;
		fout << "  " << now_time_str << endl;  // 打印时间
		fout << "[system]: '" << Clients[i].userName << "'" << " connect successfully!" << endl;

		Clients[i].flag = Clients[i].sClient;  // 不同的socket由不同UINT_PTR类型的数字来标识
		HandleRecv[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadRecv, &Clients[i].flag, 0, NULL);  // 开启接收消息的线程

		// 告诉其他用户有人上线
		char tempMessage[100] = "[system]:";
		char sys[10] = "come!";
		sprintf_s(tempMessage, "%s %s %s", tempMessage, Clients[i].userName, sys);
		for (int j = 0; j < THREAD_NUM; j++) {
			if(Clients[j].flag != 0 && j!=i)
				send(Clients[j].sClient, tempMessage, sizeof(tempMessage), 0);
		}

		Sleep(1000);
	}
	return 0;
}

int main(){
	WSADATA wsaData = { 0 };

	// 初始化套接字
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "WSAStartup error: " << WSAGetLastError() << endl;
		return -1;
	}

	// 创建套接字
	ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // IPV4、流式套接字、TCP协议
	if (ServerSocket == INVALID_SOCKET)
	{
		cout << "Socket error: " << WSAGetLastError() << endl;
		return -1;
	}

	SOCKADDR_IN ServerAddr = { 0 };  // 服务端地址
	USHORT uPort = 1818;  // 服务器监听端口
	// 设置服务器地址
	ServerAddr.sin_family = AF_INET;  // 连接方式
	ServerAddr.sin_port = htons(uPort);  //  服务器监听端口
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);  // 本机IP（监听所有接口，接收所有请求）

	// 绑定服务器
	if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "Bind error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		return -1;
	}

	// 设置监听客户端连接数
	if (SOCKET_ERROR == listen(ServerSocket, THREAD_NUM*2))
	{
		cout << "Listen error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return -1;
	}
	// 输出到文件
	fout.open("output.log", ios::app);
	fout << "***************Chat Room****************" << endl << endl;
	fout << "========================================" << endl;
	// 打印到屏幕
	cout << "***************Chat Room****************" << endl << endl;
	cout << "========================================" << endl;
	cout << "please wait for other clients to join..." << endl;
	mainHandle = (HANDLE)_beginthreadex(NULL, 0, ThreadAccept, NULL, 0, 0);   // 连接其他client的主要线程
	char Serversignal[10];
	cout << "if you want to close the server, please input 'exit' " << endl;
	cout << "========================================" << endl;
	while (true)
	{
		cin.getline(Serversignal, 10);
		if (strcmp(Serversignal, "exit") == 0)
		{
			fout << "The server is closed. " << endl << endl;  // 输出到文件
			fout.close();
			cout << "The server is closed. " << endl;  // 打印到屏幕
			CloseHandle(mainHandle);
			for (int j = 0; j < THREAD_NUM; j++)  // 依次关闭套接字
			{
				if (Clients[j].sClient != INVALID_SOCKET)
					closesocket(Clients[j].sClient);
			}
			closesocket(ServerSocket);
			WSACleanup();
			exit(1);
			return 1;
		}
	}
}