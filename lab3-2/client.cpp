#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#include<iostream>
#include<WinSock2.h>
#include<time.h>
#include<windows.h>
#include<fstream>
#include<iostream>
#include<string>
#include<mutex> 
using namespace std;
#pragma comment(lib,"ws2_32.lib")

// 初始化dll
WSADATA wsadata;

// 服务器地址
SOCKADDR_IN server_addr;
// 路由器地址
SOCKADDR_IN router_addr;
// 客户端地址
SOCKADDR_IN client_addr;
SOCKET client;

int slen = sizeof(server_addr);
int rlen = sizeof(router_addr);

char* message = new char[100000000];
char* fileName = new char[100];
unsigned long long int fileLength = 0;
unsigned long long int mPointer = 0;  // 下一个传输位置

// 常量设置
u_long unblockmode = 1;
u_long blockmode = 0;

int nextSeq = 0;  // 下一个发送序列号
int base = 0;  // 窗口基序号

int WINDOW_SIZE = 4;  // 窗口大小
int SEQ_SIZE = 8;  // 序列号大小
const unsigned short MAX_DATA_LENGTH = 0x3FF;
const int MAX_TIME = 0.2*CLOCKS_PER_SEC;  // 最大传输延迟时间

u_long IP = 0x7F000001;
const u_short SOURCE_PORT = 7776;  // 客户端端口号：7776
const u_short DES_PORT = 7778;  // 服务端端口号：7778

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

long long head,tail,freq;  // 计时器


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
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(7778);  // server的端口号
    server_addr.sin_addr.s_addr = htonl(2130706433);  //  127.0.0.1

    router_addr.sin_family = AF_INET;  
    router_addr.sin_port = htons(7777);  // router的端口号
    router_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(7776);  // client的端口号
    client_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    client = socket(AF_INET, SOCK_DGRAM, 0);
    bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));  // 绑定客户端地址
    cout << "             --------------客户端初始化成功--------------" << endl;
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
    Header header;
    clock_t start_connect = clock();
    char* shakeRBuffer = new char[sizeof(header)];
    char* shakeSBuffer = new char[sizeof(header)];
    // 将套接字设置为非阻塞模式
    ioctlsocket(client, FIONBIO, &unblockmode);

    // 第一次握手
    setHeader(header, SYN, 0, 0, 0);
    memcpy(shakeSBuffer, &header, sizeof(header));
    while(true){
        if (sendto(client, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
            cout << "[FAILED]第一次握手失败" << endl;
            return -1;
        }
        cout << "[CONNECT]第一次握手成功" << endl;

        // 设置计时器
        clock_t start = clock();
        // 等待接受第二次握手信息
        while (recvfrom(client, shakeRBuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start_connect > 60 * CLOCKS_PER_SEC) {
                cout << "[ERROR]连接超时，自动断开" << endl;
                return -1;
            }
            if (clock() - start > MAX_TIME) {
                // 重传
                cout << "[FAILED]第二次握手超时，重传第一次握手" << endl;
                if (sendto(client, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]第一次握手失败" << endl;
                    return -1;
                }
                start = clock();
                //cout << "[CONNECT]第一次握手成功" << endl;
            }
        }

        // 检验第二次握手
        memcpy(&header, shakeRBuffer, sizeof(header));
        if (header.flag == SYN_ACK && check((u_short*)&header, sizeof(header)) == 0) {
            cout << "[CONNCET]第二次握手成功" << endl;
            break;
        }
        else 
            cout << "[FAILED]数据错误，重传第一次握手" << endl;
    }

    // 发送第三次握手
    setHeader(header, ACK, 1, 0, 0);
    memcpy(shakeSBuffer, &header, sizeof(header));
    if (sendto(client, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第三次握手失败" << endl;
        return -1;
    }
    // 等待确认第三次握手成功
    clock_t start = clock();
    while(true){
        int getData = recvfrom(client, shakeRBuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, shakeRBuffer, sizeof(header));
            if (header.flag == SYN_ACK && check((u_short*)&header, sizeof(header)) == 0) {
                cout << "[FAILED]重传第三次握手" << endl;
                if (sendto(client, shakeSBuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]第三次握手失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        if(clock() - start > 2*MAX_TIME){
            cout << "[CONNECT]第三次握手成功" << endl;
            break;
        }
    }
    
    delete shakeRBuffer;
    delete shakeSBuffer;
    return 1;
}

int loadFile() {  // 读取文件
    while(true){
        cout << "[INPUT]要传输的文件：" << endl;
        cin>>fileName;
        ifstream fin(fileName, ifstream::binary);
        unsigned long long int index = 0;
        unsigned char temp = fin.get();
        // 循环读取文件
        while (fin)
        {
            message[index++] = temp;
            temp = fin.get();
        }
        fin.close();
        if(index<=0){
            cout<<"[ERROR]文件读取失败，请重新输入"<<endl;

            continue;
        }
        else{
            fileLength = index-1;
            break;
        }
    }
    cout << "[INPUT]文件读取完成" << endl;
    return 0;
}

void getTheMessage(Header &header, int length, int seq, int ack, char* sendBuffer){  // 辅助函数，填充数据包
    header.seq = seq;  // 序列号
    header.ack = ack;  // 确认号
    header.length = length;  // 数据长度
    memset(sendBuffer, 0, sizeof(header) + MAX_DATA_LENGTH);  // sendbuffer置零
    memcpy(sendBuffer, &header, sizeof(header));  // 拷贝数据头
    memcpy(sendBuffer+sizeof(header), message + mPointer, length);  // 拷贝数据内容
    mPointer += length;  // 更新数据指针
    header.checksum = getCkSum((u_short*)sendBuffer, sizeof(header)+MAX_DATA_LENGTH);  // 计算校验和
    memcpy(sendBuffer, &header, sizeof(header));  // 填充校验和（struct连续存储）
}

int endSend(int seq) {  // 发送结束信号
    Header header;
    ioctlsocket(client, FIONBIO, &unblockmode);
    char* sendbuffer = new char[sizeof(header)+MAX_DATA_LENGTH];
    char* recvbuffer = new char[sizeof(header)];

    // 设置数据报
    header.flag = OVER;
    header.seq = seq;
    header.length = strlen(fileName)+1;
    memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);  // sendbuffer置零
    memcpy(sendbuffer, &header, sizeof(header));  // 拷贝数据头
    memcpy(sendbuffer + sizeof(header), fileName, strlen(fileName)+1);  // 拷贝文件名
    header.checksum = getCkSum((u_short*)sendbuffer, sizeof(header)+MAX_DATA_LENGTH);  // 计算校验和
    memcpy(sendbuffer, &header, sizeof(header));  // 填充校验和（struct连续存储）
    cout<<"[END]发送结束信号，CheckSum："<<header.checksum<<"，Length："<<header.length<<endl;

    if (sendto(client, sendbuffer,(sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]数据包发送失败" << endl;
        return -1;
    }
    clock_t start = clock();

    while(true){
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                cout<<"[FAILED]数据包确认超时，重传"<<endl;
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]数据包" << seq << "发送失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        memcpy(&header, recvbuffer, sizeof(header));
        if (header.flag == OVER_ACK && check((u_short*)&header, sizeof(header)) == 0) {
            cout << "[END]结束发送" << endl;
            break;
        }
        else
            cout << "[FAILED]数据包错误，等待重传" << endl;
    }
    delete sendbuffer;
    delete recvbuffer;
    return 1;
}

clock_t start = clock();  // 计时器
bool startFlag = true;  // 计时器开关
mutex mtx;  // 互斥锁
bool exitThread = false;  // 退出接收线程

// 接收数据线程
unsigned __stdcall ThreadRecv(void* param){
    Header header;
    char* recvbuffer = new char[sizeof(header)];
    while(!exitThread){
        // -----接收事件-----
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            // 检查ACK
            memcpy(&header, recvbuffer, sizeof(header));
            if (header.flag == ACK && check((u_short*)&header, sizeof(header) == 0)) {
                mtx.lock();
                base = header.ack + 1;  // 累积确认
                if (base == SEQ_SIZE)
                    base = 0;
                cout << "[RECEIVE]接受服务端ACK" << header.ack << "，窗口滑动base：" << base << "，nextSeq：" << nextSeq <<endl;
                if(base == nextSeq)
                    startFlag = false;
                else{
                    start = clock();
                    startFlag = true;
                }
                mtx.unlock();                    
            }
            // 数据包错误什么也不做
        }
    }
    delete recvbuffer;
    return 0;
}

int sendMessage() {  // 发送数据，都是以MAX_DATA_LENGTH为单位发送
    QueryPerformanceCounter((LARGE_INTEGER*)&head);  // 开始计时
    ioctlsocket(client, FIONBIO, &unblockmode);  // 设置为非阻塞模式
    Header header;
    char** sendbuffer = new char*[SEQ_SIZE];  // 发送缓冲区Window
    for (int i = 0; i < SEQ_SIZE; i++)
        sendbuffer[i] = new char[sizeof(header) + MAX_DATA_LENGTH];

    _beginthreadex(NULL, 0, ThreadRecv, 0, 0, NULL);  // 启动接收线程

    while (true) {
        int thisTimeLength;  // 本次数据传输长度
        int temp = nextSeq;
        // 确认窗口范围
        mtx.lock();
        if (nextSeq < base)
            temp = nextSeq + SEQ_SIZE;
        int window_bound = base + WINDOW_SIZE;
        mtx.unlock();

        // -----发送事件-----
        if(temp < window_bound){  // 可以发送数据
            if (mPointer >= fileLength) {  // 发送完毕
                // 确定窗口都已经确认，结束发送
                if(base == nextSeq){
                    // 设置exitThread为true，通知ThreadRecv函数退出
                    exitThread = true;
                    // 清空缓冲区
                    for (int i = 0; i < SEQ_SIZE; i++)
                        delete sendbuffer[i];
                    delete sendbuffer;
                    // 结束文件传输
                    if (endSend(nextSeq) == 1)
                        return 1;
                    return -1;
                }
                // 窗口内未确认完毕，等待确认
            }
            else{
                // 封装数据包
                if (fileLength - mPointer >= MAX_DATA_LENGTH)  // 可以按照最大限度发送
                    thisTimeLength = MAX_DATA_LENGTH;
                else 
                    thisTimeLength = fileLength - mPointer + 1;  // 计算有效数据长度
                getTheMessage(header,thisTimeLength,nextSeq,nextSeq,sendbuffer[nextSeq]);

                // 发送数据包，补满数据包一起发送
                cout << "[SEND]发送" << nextSeq << "号数据包，数据包大小：" << thisTimeLength;
                cout << "，ACK:" << header.ack <<"，CheckSum："<<header.checksum << endl;
                if (sendto(client, sendbuffer[nextSeq], (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]数据包" << nextSeq << "发送失败" << endl;
                    return -1;
                }

                mtx.lock();
                // 如果是第一个数据包，设置计时器
                if(base == nextSeq){
                    start = clock();
                    startFlag = true;
                }
                // 转变序号
                nextSeq++;
                if(nextSeq == SEQ_SIZE)
                    nextSeq = 0;
                mtx.unlock();
            }
        }

        // -----超时事件-----
        if (startFlag && (clock() - start > MAX_TIME*2)) {
            cout<<"[FAILED]数据包确认超时，重传窗口所有数据包"<<endl;
            // 确认窗口范围
            int temp = nextSeq;
            mtx.lock();
            int temp_base = base;
            mtx.unlock();

            if (temp_base > nextSeq)
                temp = SEQ_SIZE;
            for(int i=temp_base;i<temp;i++){
                cout<<"[RESEND]重传"<<i<<"号数据包"<<endl;
                if (sendto(client, sendbuffer[i], (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]数据报" << i<< "发送失败" << endl;
                    return -1;
                }
            }
            if (temp == SEQ_SIZE){
                for(int i=0;i<nextSeq;i++){
                    cout<<"[RESEND]重传"<<i<<"号数据包"<<endl;
                    if (sendto(client, sendbuffer[i], (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                        cout << "[FAILED]数据报" << i<< "发送失败" << endl;
                        return -1;
                    }
                }
            }
            // 重置计时器
            mtx.lock();
            start = clock();
            startFlag = true;
            mtx.unlock();
        }
    }
}

int disconnect() {  // 三次挥手断开连接
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);  // 结束计时
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
    ioctlsocket(client, FIONBIO, &unblockmode);
    setHeader(header, FIN, 0, 0, 0);

    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第一次挥手发送失败" << endl;
        return -1;
    }
    cout << "[DISCONNECT]第一次挥手发送成功" << endl;

    // 接收第二次挥手
    clock_t start = clock();
    while (true) {
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if (getData > 0) {
            memcpy(&header, recvbuffer, sizeof(header));
            if (header.flag == FIN_ACK && check((u_short*)&header, sizeof(header)) == 0) {
                cout << "[DISCONNECT]收到第二次挥手消息" << endl;
                break;
            }
        }
        if (clock() - start > MAX_TIME) {
            cout<<"[FAILED]第二次挥手超时，重发第一次挥手"<<endl;
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                cout << "[FAILED]第一次挥手发送失败" << endl;
                return -1;
            }
            start = clock();
        }
    }

    // 发送第三次挥手
    setHeader(header, ACK, 1, 0, 0);
    memcpy(sendbuffer, &header, sizeof(header));

    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第三次挥手发送失败" << endl;
        return -1;
    }
    start = clock();

    while (true) {
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, recvbuffer, sizeof(header));
            if (header.flag == FIN_ACK && check((u_short*)&header, sizeof(header)) == 0) {  // 再次收到第二次挥手
                cout << "[FAILED]重传第三次挥手" << endl;
                if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]第三次挥手发送失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        if (clock() - start > 4 * MAX_TIME) {
            // 等待一段时间，如果没有收到第二次挥手，认为服务器已经断开连接
            cout << "[DISCONNECT]成功断开连接" << endl;
            break;
        }
    }
    delete sendbuffer;
    delete recvbuffer;
    return 1;
}

void printLog() {  // 打印日志
    cout << "             --------------传输日志--------------" << endl;
    cout << "报文总长度：" << mPointer << "字节，分为" << (mPointer / 1024) + 1 << "个报文段分别发送" << endl;
    double t = (double)(tail - head) / freq;
    cout << "传输用时：" <<t <<"秒"<< endl;
    cout << "报文平均往返时延为" << t / (mPointer / 1024 + 1) * 1000 << "毫秒" << endl;
    t = (double)mPointer / t;
    cout << "传输吞吐率为" << t <<"字节每秒"<< endl;
}

int main() {
    init();
    while(true){
        cout<<"[INPUT]输入窗口大小："<<endl;
        cin>> WINDOW_SIZE;
        SEQ_SIZE = WINDOW_SIZE * 2;

        connect();
        loadFile();
        sendMessage();
        disconnect();
        printLog();

        fileLength = 0;
        mPointer = 0;
        nextSeq = 0;
        base = 0;
        exitThread = false;
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