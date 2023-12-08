#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<windows.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

// 初始化dll
WSADATA wsadata;

// 服务器套接字及地址
SOCKADDR_IN server_addr;
SOCKET server;
// 路由器套接字及地址
SOCKADDR_IN router_addr;;
// 客户端地址
SOCKADDR_IN client_addr;

int clen = sizeof(client_addr);
int rlen = sizeof(router_addr);

char* message = new char[100000000];
char* fileName = new char[100];
unsigned long long int mPointer;

// 常数设置
u_long blockmode = 0;
u_long unblockmode = 1;

int rcvBase = 0;  // 期待的序列号

int WINDOW_SIZE = 4;  // 窗口大小
int SEQ_SIZE = 8;  // 序列号大小
const unsigned short MAX_DATA_LENGTH = 0x1000;

u_long IP = 0x7F000001;
const u_short SOURCE_PORT = 7778;  // 源端口7778
const u_short DES_PORT = 7776;  // 客户端端口号7776
const int MAX_TIME = 0.5*CLOCKS_PER_SEC;  // 最大传输延迟时间

const unsigned char SYN = 0x1;  // 00000001
const unsigned char ACK = 0x2;  // 00000010
const unsigned char SYN_ACK = 0x3;  // 00000011
const unsigned char OVER = 0x4;  // 00000100
const unsigned char OVER_ACK = 0x6;  // 00000110
const unsigned char FIN = 0x8;  // 00001000
const unsigned char FIN_ACK = 0xA;  // 00001010

// 数据头
struct Header {
    u_short checksum;  // 16位校验和
    u_short seq;  // 16位序列号，rdt3.0，只有最低位0和1两种状态
    u_short ack;  // 16位ack号，ack用来做确认
    u_short flag;  // 16位状态位 FIN,OVER,ACK,SYN
    u_short length;  // 16位长度位
    u_short source_port;  // 16位源端口号
    u_short des_port;  // 16位目的端口号

    Header() {
        checksum = 0;
        source_port = SOURCE_PORT;
        des_port = DES_PORT;
        seq = 0;
        ack = 0;
        flag = 0;
        length = 0;
    }
};


u_short getCkSum(u_short* mes, int size) {
    // size为字节数，ushort16位2字节（向上取整）
    int count = (size + 1) / 2;
    u_short* buf = new u_short[size + 1];
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);

    u_long sum = 0;
    sum += ((IP >> 16) & 0xffff + IP & 0xffff)*2;  // 伪首部
    // 跳过校验和字段
    buf ++;
    count -= 1;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {  // 溢出则回卷
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

u_short check(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = new u_short[size + 1];
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
    
    u_long sum = 0;
    sum += ((IP >> 16) & 0xffff + IP & 0xffff)*2;  // 伪首部
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

void init() {  // 初始化
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7778);  // server的端口号
    server_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(7777);  // router的端口号
    router_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(7776);  // client的端口号
    client_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    server = socket(AF_INET, SOCK_DGRAM, 0);
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));  // 绑定地址

    cout << "             --------------服务器初始化成功--------------" << endl;
}

void setHeader(Header& header, unsigned char flag, unsigned short seq, unsigned short ack, unsigned short length) {
    header.source_port = SOURCE_PORT;
    header.des_port = DES_PORT;
    header.flag = flag;
    header.seq = seq;
    header.ack = ack;
    header.length = length;
    header.checksum = getCkSum((u_short*)&header, sizeof(header));
}

int connect() {  // 三次握手连接
    clock_t veryBegin = clock();
    Header header;
    char* shakeRBuffer = new char[sizeof(header)];
    char* shakeSBuffer = new char[sizeof(header)];
    cout << "[CONNECT]正在等待连接" << endl;
    ioctlsocket(server, FIONBIO, &unblockmode);

    // 等待第一次握手
    while (true) {
        int getData = recvfrom(server, shakeRBuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, shakeRBuffer, sizeof(header));  // 给数据头赋值
            // 检验握手数据包是否正确
            if (header.flag == SYN && check((u_short*)(&header), sizeof(header)) == 0) {
                cout << "[CONNECT]第一次握手成功" << endl;
                break;
            }
            // 数据错误，继续等
        }
        if (clock() - veryBegin > 60 * CLOCKS_PER_SEC) {
            cout << "[ERROR]连接超时，自动断开" << endl;
            return -1;
        }
    }

    // 发送第二次握手
    setHeader(header,SYN_ACK,0,0,0);
    memcpy(shakeSBuffer, &header, sizeof(header));
    if (sendto(server, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第二次握手发送失败" << endl;
        return -1;
    }
    cout << "[CONNECT]第二次握手成功" << endl;

    // 等待第三次握手
    clock_t start = clock();
    while (true) {  
        int getData = recvfrom(server, shakeRBuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, shakeRBuffer, sizeof(header));  // 给数据头赋值
            // 检验握手数据包是否正确
            if (header.flag == ACK && check((u_short*)(&header), sizeof(header)) == 0) {
                cout << "[CONNECT]第三次握手成功" << endl;
                break;
            }
            // 数据错误，继续等
        }
        if (clock() - veryBegin > 60 * CLOCKS_PER_SEC) {
            cout << "[ERROR]连接超时，自动断开" << endl;
            return -1;
        }
        if (clock() - start > MAX_TIME) {
            cout << "[FAILED]第三次握手超时，重传第二次握手" << endl;
            if (sendto(server, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                cout << "[FAILED]第二次握手发送失败" << endl;
                return -1;
            }
            start = clock();
        }
    }
    delete shakeRBuffer;
    delete shakeSBuffer;
    return 1;
}

int dumpFile() {  // 保存文件
    char* rFileName = new char[102];
    sprintf(rFileName, "r_%s", fileName);
    cout << "             --------------传输结束--------------"<<endl;
    ofstream fout(rFileName, ofstream::binary);
    for (int i = 0; i < mPointer; i++)
    {
        fout << message[i];
    }
    fout.close();
    cout << "文件已被保存为 "<<rFileName << endl;
    return 0;
}

int endReceive() {  // 发送OVER_ACK信号
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    header.flag = OVER_ACK;
    header.checksum = getCkSum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header,sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]确认消息发送失败" << endl;
        return -1;
    }
    // 在disconnect检测，如果收到的是OVER，就再重传一次OVER_ACK
    cout << "[END]"<<fileName<<"接收完毕" << endl;
    delete sendbuffer;
    return 1;
}


char** winBuffer;
bool* isRecv;

void moveWindow(){
    Header header;
    while(isRecv[rcvBase] == true){
        cout<<"[MOVE]滑动窗口，交付"<<rcvBase<<"号数据包"<<endl;
        memcpy(&header, winBuffer[rcvBase], sizeof(header));
        memcpy(message + mPointer, winBuffer[rcvBase] + sizeof(header), header.length);
        mPointer += header.length;
        isRecv[rcvBase++] = false;
        if(rcvBase == SEQ_SIZE)
            rcvBase = 0;
    }
}

int receiveMessage() {  // 接收消息，接收到错误或冗余就重传
    ioctlsocket(server, FIONBIO, &unblockmode);
    Header header;
    isRecv = new bool[SEQ_SIZE];
    winBuffer = new char*[SEQ_SIZE];
    for (int i = 0; i < SEQ_SIZE; i++){
        winBuffer[i] = new char[sizeof(header) + MAX_DATA_LENGTH];
        isRecv[i] = false;
    }
    char* recvbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    char* sendbuffer = new char[sizeof(header)];

    clock_t start = clock();
    // 接受数据
    while (true) {
        // 等待接收数据
        int getData = recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            start = clock();
            memcpy(&header, recvbuffer, sizeof(header));  // 给数据头赋值
            cout<<"[RECEIVE]收到"<<header.seq<<"号数据包，CheckSum："<<header.checksum;
            cout<<"，Seq:"<<header.seq<<"，Ack:"<<header.ack<<"，Length:"<<header.length<<endl;

            // 检验数据包是否正确
            if(check((u_short*)recvbuffer,sizeof(header)+MAX_DATA_LENGTH)==0){  // 数据包正确 
                if(header.flag == OVER){
                    // 提取文件名
                    memcpy(fileName, recvbuffer + sizeof(header), header.length);
                    delete recvbuffer;
                    delete sendbuffer;
                    delete isRecv;
                    for(int i=0;i<SEQ_SIZE;i++)
                        delete winBuffer[i];
                    delete winBuffer;
                    if(endReceive()==1)
                        return 1;
                    return -1;
                }
                int temp = header.seq;
                if(temp < rcvBase){
                    temp += SEQ_SIZE;
                }
                if(temp >= rcvBase && temp <= rcvBase + WINDOW_SIZE -1 && isRecv[header.seq] == false){
                    cout << "[RECEIVE]数据包正确，缓存" << header.seq << endl;
                    // 在接收窗口内，需要缓存失序分组/滑动窗口
                    isRecv[header.seq] = true;
                    memcpy(winBuffer[header.seq], recvbuffer, sizeof(header) + MAX_DATA_LENGTH);
                    if(header.seq == rcvBase)
                        // 滑动窗口
                        moveWindow();
                }
                // 发送ACK，seqSize=2*winSize，肯定可以发
                setHeader(header,ACK,header.seq,header.seq,0);
                memcpy(sendbuffer, &header, sizeof(header));
                if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]ACK发送失败" << endl;
                    return -1;
                }
                cout << "[SEND]发送ACK" << header.seq << endl;
            }
        }
        if (clock() - start > 30 * CLOCKS_PER_SEC) {
            cout << "[ERROR]连接超时，自动断开" << endl;
            return -1;
        }
    }
}

int disconnect() {  // 三次挥手断开连接
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
    clock_t start = clock();
    
    while(true){
        int getData = recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, recvbuffer, sizeof(header));
            // 如果收到的是OVER，就再重传一次OVER_ACK
            if(header.flag == OVER){
                if(check((u_short*)recvbuffer,sizeof(header)+MAX_DATA_LENGTH) == 0){
                    cout << "[END]重传OVER_ACK" << endl;
                    endReceive();
                }
                continue;
            }
            // 收到第一次挥手信息
            else if(header.flag == FIN && check((u_short*)&header, sizeof(header)) == 0){
                cout << "[DISCNNECT]收到第一次挥手信息" << endl;
                break;
            }
        }
        if(clock() - start > 20 * CLOCKS_PER_SEC){
            cout << "[ERROR]连接超时，自动断开" << endl;
            return -1;
        }
    }

    // 发送第二次挥手信息
    setHeader(header,FIN_ACK,0,0,0);
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第二次挥手信息发送失败" << endl;
        return -1;
    }
    cout << "[DISCNNECT]发送第二次挥手信息" << endl;

    start = clock();
    while(true){
        // 等待第三次挥手信息
        int getData = recvfrom(server, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, recvbuffer, sizeof(header));
            // 第三次挥手ACK已经收到
            if(header.flag == ACK && check((u_short*)&header, sizeof(header)) == 0){
                cout << "[DISCNNECT]收到第三次挥手信息" << endl;
                break;
            }
        }
        // 超时重传第二次挥手信息
        if(clock() - start > MAX_TIME){
            cout << "[FAILED]重传第二次挥手" << endl;
            if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                cout << "[FAILED]第二次挥手信息发送失败" << endl;
                return -1;
            }
            start = clock();
        }
    }
    cout<<"[DISCNNECT]连接已断开"<<endl;

    delete sendbuffer;
    delete recvbuffer;
    return 1;
}

int main() {
    init();
    while(true){
        cout<<"[INPUT]输入窗口大小："<<endl;
        cin>> WINDOW_SIZE;
        SEQ_SIZE = WINDOW_SIZE * 2;

        connect();
        receiveMessage();
        disconnect();
        dumpFile();

        mPointer = 0;
        rcvBase = 0;
        memset(message, 0, sizeof(message));
        memset(fileName, 0, sizeof(fileName));

        cout<<"[INPUT]输入q退出，其他键继续"<<endl;
        char c;
        cin>>c;
        if(c == 'q')
            break;
        system("cls");
    }
    system("pause");
    return 0;
}