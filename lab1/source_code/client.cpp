#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <iostream>
#include <time.h>

using namespace std;
#pragma comment(lib,"ws2_32.lib")

char userName[16] = { 0 };
#define uPort 1818
#define IP "127.0.0.1"

time_t now_time;  // 用来打印时间
char now_time_str[20];


// 接收数据线程
unsigned __stdcall ThreadRecv(void* param)
{
	char bufferRecv[128] = { 0 };
	while (true)
	{
		if (recv(*(SOCKET*)param, bufferRecv, sizeof(bufferRecv), 0) == SOCKET_ERROR)
		{
			Sleep(500);
			continue;
		}
		if (strlen(bufferRecv) != 0)
		{
			// 覆盖之前的用户名
			for (int i = 0; i <= strlen(userName) + 26; i++)
				cout << "\b";
			// 打印接收到的信息
			time(&now_time);  // 打印时间
			strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
			cout << "(" << now_time_str << ") ";
			cout << bufferRecv << endl;
			// 重新打印userName，因为之前覆盖掉了
			time(&now_time);  // 打印时间
			strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
			cout << "(" << now_time_str << ") ";
			cout << "[" << userName << "]: ";  // 打印用户名
		}
		else
			Sleep(100);
	}
	return 0;
}


int main()
{
	WSADATA wsaData = { 0 };  // 存放套接字信息
	SOCKET ClientSocket = INVALID_SOCKET;  // 客户端套接字
	SOCKADDR_IN ServerAddr = { 0 };  // 服务器地址
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))  // 初始化
	{
		cout << "WSAStartup error: " << WSAGetLastError() << endl;
		return -1;
	}

	// 创建套接字
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ClientSocket == INVALID_SOCKET)
	{
		cout << "Socket error: " << WSAGetLastError() << endl;
		return -1;
	}

	// 输入服务器IP
	/*cout << "Please input the server IP:";
	char IP[32] = { 0 };
	cin.getline(IP,32);*/

	// 设置服务器地址
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(uPort);  // 服务器端口
	ServerAddr.sin_addr.S_un.S_addr = inet_addr(IP);  // 服务器地址

	cout << "***************Chat Room****************" << endl << endl;
	cout << "========================================" << endl;
	cout << "connecting...please wait..." << endl;
	// 连接服务器
	if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))  // OS自动为客户端套接字分配IP和Port
	{
		cout << "Connect error: " << WSAGetLastError() << endl;
		closesocket(ClientSocket);
		WSACleanup();
		return -1;
	}
	cout << "Connect successfully! " << endl;
	cout << "The server's ip is:" << inet_ntoa(ServerAddr.sin_addr) << "  The server's port is:" << htons(ServerAddr.sin_port) << endl;  // 转成字符串输出
	// 打印时间
	time(&now_time);
	strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
	cout << now_time_str << endl;
	cout << "========================================" << endl;

	// 打印可聊天对象
	char nameList[300] = { 0 };
	recv(ClientSocket, nameList, sizeof(nameList), 0);  // 从套接字接收数据
	cout << nameList << endl;
	cout << "========================================" << endl;

	// 接收用户名
	cout << "Please input your name: ";
	cin.getline(userName, 16);
	cout << "Hi " << userName << "!" << endl;
	cout << "========================================" << endl;
	send(ClientSocket, userName, sizeof(userName), 0);

	// 接收聊天对象
	char buddy[16] = { 0 };
	cout << "You can choose other clients to chat:" << endl;
	cout << "    - If you input 'all', then everyone can receive your message" << endl;
	cout << "    - If you input the client's name, then the only client can receive your message" << endl;

	cout << "please choose the client: ";
	cin.getline(buddy, 16);  // 用户输入聊天对象
	cout << "========================================" << endl;
	send(ClientSocket, buddy, sizeof(buddy), 0);
	_beginthreadex(NULL, 0, ThreadRecv, &ClientSocket, 0, NULL);  // 启动接收线程

	Sleep(100);
	system("cls");  // 清屏开启聊天页面
	cout << "***************Chat Room****************" << endl << endl;
	cout << "========================================" << endl;

	// 接收用户输入并发送
	char sendBuffer[128] = { 0 };
	while (1)
	{
		time(&now_time);  // 打印时间
		strftime(now_time_str, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_time));
		cout << "(" << now_time_str << ") ";
		cout << "[" << userName << "]: ";  // 打印用户名
		cin.getline(sendBuffer, 128);
		if (strcmp(sendBuffer, "exit") == 0)
		{
			cout << "you are going to exit... " << endl;
			Sleep(500);
			if (send(ClientSocket, sendBuffer, sizeof(sendBuffer), 0) == SOCKET_ERROR)
				return -1; 
			break;
		}
		// 正常输入
		if (send(ClientSocket, sendBuffer, sizeof(sendBuffer), 0) == SOCKET_ERROR)
			return -1;
	}

	closesocket(ClientSocket);
	WSACleanup();
	exit(1);
	return 0;
}