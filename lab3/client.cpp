#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#include<iostream>
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<string>
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
char* filename = new char[30];
unsigned long long int messagelength = 0;  // 最后传输的下标
unsigned long long int messagepointer = 0;  // 下一个传输位置

// 常量设置
u_long unlockmode = 1;
u_long lockmode = 0;
const unsigned char MAX_DATA_LENGTH = 0xff;
const u_short SOURCE_IP = 0x7f01;
const u_short DES_IP = 0x7f01;
const u_short SOURCE_PORT = 8887;  // 客户端端口号：8887
const u_short DES_PORT = 8888;  // 服务端端口号：8888
// FIN,OVER,FIN,ACK,SYN
const unsigned char SYN = 0x1;  // 00000001
const unsigned char ACK = 0x2;  // 00000010
const unsigned char SYN_ACK = 0x3;  // 00000011
const unsigned char OVER = 0x8;  // 00001000
const unsigned char OVER_ACK = 0xA;  // 00001010
const unsigned char FIN = 0x10;  // 00010000
const unsigned char FIN_ACK = 0x12;  // 00010010

const int MAX_TIME = 0.5*CLOCKS_PER_SEC;  // 最大传输延迟时间

// 数据头
struct Header {
    u_short checksum;  // 16位校验和
    u_short seq;  // 16位序列号，rdt3.0，只有最低位0和1两种状态
    u_short ack;  // 16位ack号
    u_short flag;  // 16位状态位 FC,FIN,OVER,FIN,ACK,SYN
    u_short length;  // 16位长度位
    u_short source_ip;  // 16位源ip地址
    u_short des_ip;  // 16位目的ip地址
    u_short source_port;  // 16位源端口号
    u_short des_port;  // 16位目的端口号

    Header() {
        checksum = 0;
        source_ip = SOURCE_IP;
        des_ip = DES_IP;
        source_port = SOURCE_PORT;
        des_port = DES_PORT;
        seq = 0;
        ack = 0;
        flag = 0;
        length = 0;
    }
};

// 全局时钟设置
clock_t veryBegin;
clock_t ALLEND;


// sizeof返回内存字节数 ushort是16位2字节，所以需要把size向上取整
u_short getCkSum(u_short* mes, int size) {
    // TODO：check
    // 计算16位校验和
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
    u_long sum = 0;
    buf += 1;
    count -= 1;
    while (count--) {
        sum += *(buf++);
        if (sum & 0xffff0000) {  // 溢出则回卷
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

u_short check(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
    u_long sum = 0;
    //buf += 0;
    //count -= 0;
    while (count--) {
        sum += *(buf++);
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
    server_addr.sin_port = htons(8888);  // server的端口号
    server_addr.sin_addr.s_addr = htonl(2130706433);  //  127.0.0.1

    router_addr.sin_family = AF_INET;  
    router_addr.sin_port = htons(8886);  // router的端口号
    router_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8887);  // client的端口号
    client_addr.sin_addr.s_addr = htonl(2130706433);  // 127.0.0.1

    client = socket(AF_INET, SOCK_DGRAM, 0);
    bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));  // 绑定客户端地址
    cout << "[INIT]初始化成功" << endl;
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
    veryBegin = clock();
    Header header;
    char* shakeRBuffer = new char[sizeof(header)];
    char* shakeSBuffer = new char[sizeof(header)];
    // 将套接字设置为非阻塞模式 TODO：check
    ioctlsocket(client, FIONBIO, &unlockmode);

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
            if (clock() - veryBegin > 60 * CLOCKS_PER_SEC) {
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
    setHeader(header, ACK, 1, 1, 0);
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
    string file ;
    cout << "[INPUT]要传输的文件：" << endl;
    cin >> file;
    strcpy(filename, file.c_str());
    ifstream fin(file.c_str(), ifstream::binary);
    unsigned long long int index = 0;
    unsigned char temp = fin.get();
    // 循环读取文件
    while (fin)
    {
        message[index++] = temp;
        temp = fin.get();
    }
    messagelength = index-1;
    fin.close();
    cout << "[INPUT]文件读取完成" << endl;
    return 0;
}

void getTheMessage(Header &header, int length, int seq, char* sendBuffer){  // 辅助函数，填充数据包
    header.seq = seq;  // 序列号
    header.length = length;  // 数据长度
    memset(sendBuffer, 0, sizeof(header) + MAX_DATA_LENGTH);  // sendbuffer置零
    memcpy(sendBuffer, &header, sizeof(header));  // 拷贝数据头
    memcpy(sendBuffer+sizeof(header), message + messagepointer, length);  // 拷贝数据内容
    messagepointer += length;  // 更新数据指针
    header.checksum = getCkSum((u_short*)sendBuffer, sizeof(header)+MAX_DATA_LENGTH);  // 计算校验和
    memcpy(sendBuffer, &header, sizeof(header));  // 填充校验和（struct连续存储）
}

int sendMessage() {  // 发送数据
    veryBegin = clock();
    // 设置为非阻塞模式
    ioctlsocket(client, FIONBIO, &unlockmode);
    Header header;
    char* recvbuffer = new char[sizeof(header)];
    char* sendbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    int seq = 0;

    while (true) {
        int thisTimeLength;  // 本次数据传输长度
        if (messagepointer > messagelength) {  // 发送完毕
            delete recvbuffer;
            delete sendbuffer;
            if (endsend() == 1)
                return 1;
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH)  // 可以按照最大限度发送
            thisTimeLength = MAX_DATA_LENGTH;
        else 
            thisTimeLength = messagelength - messagepointer + 1;  // 计算发送长度
        getTheMessage(header,thisTimeLength,seq,sendbuffer);

        // 发送数据包
        cout << "[SEND]准备发送" << seq << "号数据包，数据包大小:" << thisTimeLength<<" ";
        cout << "数据包校验和：" << check((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;

        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
            cout << "[FAILED]数据包发送失败" << endl;
            return -1;
        }
        // 开始计时器
        clock_t start = clock();
        // 确认接收ACK，超时重传
        while (true) {
            int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
            if(getData > 0){
                // 检查ACK
                memcpy(&header, recvbuffer, sizeof(header));
                //cout << "[SEND]接受到的ack：" << header.ack << " 校验和：" << check((u_short*)&header, sizeof(header)) << endl;
                if (header.ack == seq && check((u_short*)&header, sizeof(header) == 0)) {
                    cout << "[SEND]接受服务端ACK，准备发送下一数据包" << endl;
                    break;
                }
            }
            if (clock() - start > MAX_TIME) {
                cout<<"[FAILED]数据包确认超时，重传"<<endl;
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]数据报发送失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        // 转变序号
        if(seq == 0)
            seq = 1;
        else
            seq = 0;
    }
}

int endsend() {  // 发送结束信号
    ALLEND = clock();
    Header header;
    ioctlsocket(client, FIONBIO, &unlockmode);
    char* sendbuffer = new char[sizeof(header)+MAX_DATA_LENGTH];
    char* recvbuffer = new char[sizeof(header)];

    // 设置数据报
    memset(sendbuffer, 0, sizeof(header)+MAX_DATA_LENGTH);
    header.flag = OVER;
    header.length = strlen(filename);
    header.checksum = getCkSum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    // 文件名放在末尾
    memcpy(sendbuffer + sizeof(header), filename, strlen(filename));

    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]数据包发送失败" << endl;
        return -1;
    }
    cout << "[END]结束信号发送成功" << endl;
    clock_t start = clock();

    while(true){
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                cout<<"[FAILED]数据包确认超时，重传"<<endl;
                if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]数据包发送失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        memcpy(&header, recvbuffer, sizeof(header));
        if (header.flag == OVER_ACK && check((u_short*)&header, sizeof(header)) == 0) {
            cout << "[END]传输结束消息发送成功" << endl;
            break;
        }
        else
            cout << "[FAILED]数据包错误，等待重传" << endl;
    }
    delete sendbuffer;
    delete recvbuffer;
    return 1;
}

int disconnect() {  // 四次挥手断开连接
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
    ioctlsocket(client, FIONBIO, &unlockmode);
    setHeader(header, FIN, 0, 0, 0);

    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[DISCONNECT]第一次挥手发送失败" << endl;
        return -1;
    }

    // 接收第二/三次挥手
    clock_t start = clock();
    while (true) {
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if (getData > 0) {
            memcpy(&header, recvbuffer, sizeof(header));
            if (header.flag == ACK && check((u_short*)&header, sizeof(header)) == 0) {
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
    while(true){
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        memcpy(&header, recvbuffer, sizeof(header));
        if (header.flag == FIN_ACK && check((u_short*)&header, sizeof(header)) == 0)
            cout << "[DISCONNECT]收到第三次挥手消息" << endl;
        if(clock() - start > 10*CLOCKS_PER_SEC){  // 怕死锁，10s超时
            cout<<"[ERROR]连接超时，自动断开"<<endl;
            return -1;
        }
    }

    // 发送第四次挥手
    setHeader(header, ACK, 1, 1, 0);
    memcpy(sendbuffer, &header, sizeof(header));

    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
        cout << "[FAILED]第四次挥手发送失败" << endl;
        return -1;
    }
    start = clock();

    while (true) {
        int getData = recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen);
        if(getData > 0){
            memcpy(&header, recvbuffer, sizeof(header));
            if (header.flag == FIN_ACK && check((u_short*)&header, sizeof(header)) == 0) {  // 再次收到第三次挥手
                cout << "[FAILED]重传第四次挥手" << endl;
                if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == SOCKET_ERROR) {
                    cout << "[FAILED]第四次挥手发送失败" << endl;
                    return -1;
                }
                start = clock();
            }
        }
        if (clock() - start > 4 * MAX_TIME) {
            // 等待一段时间，如果没有收到第三次挥手，认为服务器已经断开连接
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
    cout << "报文总长度：" << messagepointer << "字节，分为" << (messagepointer / 256) + 1 << "个报文段分别转发" << endl;
    double t = (ALLEND - veryBegin) / CLOCKS_PER_SEC;
    cout << "传输用时：" <<t <<"秒"<< endl;
    t = messagepointer / t;
    cout << "传输吞吐率为" << t <<"字节每秒"<< endl;
}

int main() {
    init();
    connect();
    loadFile();
    sendMessage();
    disconnect();
    printLog();
    system("pause");
    return 0;
}