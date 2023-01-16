#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

SOCKADDR_IN serverAddr, clientAddr;
int clientAddrLen = sizeof(clientAddr);
SOCKET serverSocket;
const int SERV_PORT = 8000;
const int CLIT_PORT = 8100;
const int WAIT_TIME = 500;
const int BUF_SIZE = 1024;//传输缓冲区最大长度
const int RESEND_TIMES = 30;
const int HEADER_SIZE = 22;
const unsigned char SYN = 0x1;
const unsigned char ACK = 0x2;
const unsigned char SYN_ACK = 0x3;
const unsigned char FIN = 0x4;
const unsigned char SEND_FILE = 0x8;//发送文件
const unsigned char OVER_FILE = 0x16;//结束标志

struct message
{
#pragma pack(1)
    DWORD sendIp; //32位，发送ip 
    DWORD recvIp; //32位，接收ip 
    u_short sendPort; //16位，发送端口号 
    u_short recvPort; //16位，接收端口号 
    u_short filesize = 0;//16位，数据部分的长度 
    u_short flags = 0;//16位
    u_short seq = 0; //16位，传输的序列号，0~65535
    u_short ack = 0; //16位，用于确认的序列号
    u_short checksum = 0;//16位,校验和
    char msg[BUF_SIZE];//报文内容
#pragma pack()
    message() {
        checksum = 0;
        filesize = 0;
        flags = 0;
        seq = 0;
        ack = 0;
        sendPort = SERV_PORT;
        recvPort = CLIT_PORT;
        sendIp = serverAddr.sin_addr.s_addr;
        recvIp = clientAddr.sin_addr.s_addr;
        memset(this->msg, 0, BUF_SIZE);
    }
    u_short setCheckSum();
    int akCheckSum();
    void setFlags(int flags) {
        this->flags = flags;
    }
    int getFlags() {
        return this->flags;
    }
};

//计算校验和
u_short message::setCheckSum() {
    int sum = 0;
    u_short* temp = (u_short*)this;
    for (int i = 0; i < 10; i++) {//每16bit一组，取前10组相加
        sum += temp[i];
        while (sum >= 0x10000)//发生溢出
        {
            int a = sum >> 16; //最高位
            sum += a;
        }
    }
    this->checksum = ~(u_short)sum; //按位取反
    return this->checksum;
}

//检验校验和
int message::akCheckSum() {
    int sum = 0;
    u_short* temp = (u_short*)this;
    for (int i = 0; i < 10; i++)
    {
        sum += temp[i];
        while (sum >= 0x10000)
        {
            int a = sum >> 16;
            sum += a;
        }
    }
    //计算出的sum和校验和相加，等于0xffff则说明校验成功
    if ((u_short)this->checksum + (u_short)sum == 65535)
        return 1;
    return -1;
}

int Connect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    //接收第一次握手
    while (1) {
        if (recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen) > 0) {
            ::memcpy(&msg, BUFFER, sizeof(msg));
            if (msg.getFlags() == SYN && msg.akCheckSum() == 1)
            {
                cout << "接收到第一次握手信息~" << endl;
                break;
            }
        }
    }

    //发送第二次握手信息
    msg.setFlags(ACK);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
    start = clock();//记录第二次握手发送时间

    //接收第三次握手
    int resend_count = 0;
    while (recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen) <= 0)
    {
        if (resend_count > RESEND_TIMES)return -1;
        end = clock();
        if ((end - start) / CLOCKS_PER_SEC >= WAIT_TIME)
        {
            msg.setFlags(ACK);
            msg.setCheckSum();
            ::memcpy(BUFFER, &msg, sizeof(msg));
            sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
            resend_count++;
            cout << "第二次握手超时，正在进行重传..." << endl;
        }
    }

    //接收到第三次握手
    message msg1;
    ::memcpy(&msg1, BUFFER, sizeof(msg1));
    if (msg1.getFlags() == SYN_ACK && msg1.akCheckSum() == 1)
    {
        cout << "第三次握手成功~" << endl;
        cout << "成功建立通信！可以接收数据" << endl;
    }
    else
    {
        return -1;
    }
    return 1;
}

int RecvMessage(char* text)
{
    long int file = 0;//文件长度
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    int ack = 0;

    while (1)
    {
        recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen);//接收报文长度
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.akCheckSum() == 1) {
            if (msg.getFlags() == SEND_FILE)
            {
                //判断接收的是正确的包吗
                if (ack != int(msg.seq))
                {
                    //不是正确的包，重发当前包的ACK，并丢弃该数据包
                    msg.setFlags(ACK);
                    msg.filesize = 0;
                    msg.ack = (u_short)msg.seq;
                    msg.setCheckSum();
                    ::memcpy(BUFFER, &msg, sizeof(msg));
                    sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
                    cout << "成功发送确认数据包~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
                    continue;
                }

                ack = int(msg.seq);
                ack %= 65535;

                //取出BUFFER中的内容
                cout << "成功接收 " << int(msg.filesize) << " bytes 数据！ Flags:" << hex << "0x" << int(msg.getFlags()) << "(SEND_FILE)" << " Seq number:" << dec << int(msg.seq) << endl;
                ::memcpy(text + file, BUFFER + HEADER_SIZE, int(msg.filesize));
                text[file + msg.filesize] = '\0';
                file = file + int(msg.filesize);

                //发送ACK包
                msg.setFlags(ACK);
                msg.filesize = 0;
                msg.ack = (u_short)ack;
                msg.setCheckSum();
                ::memcpy(BUFFER, &msg, sizeof(msg));
                sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
                cout << "成功发送确认数据包~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
                ack++;
                ack %= 65535;
            }

            //是结束吗
            else if (msg.getFlags() == OVER_FILE)
            {
                cout << "已完成文件接收！" << endl;
                break;
            }
        }
    }

    //发送OVER_FILE的确认包
    msg.setFlags(ACK);
    msg.setCheckSum();
    msg.ack = (u_short)ack;
    ::memcpy(BUFFER, &msg, sizeof(msg));
    if (sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen) == -1)
    {
        return -1;
    }
    cout << "成功发送确认数据包~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
    return file;
}

int disConnect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    while (1)
    {
        recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen);//接收报文长度
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.getFlags() == FIN && msg.akCheckSum() == 1)
        {
            cout << "成功接收第一次挥手信息~" << endl;
            break;
        }
    }

    //发送第二次挥手
    msg.setFlags(ACK);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    if (sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen) == -1)
    {
        return -1;
    }
    cout << "连接断开!";
    return 1;
}


int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup错误：" << WSAGetLastError() << endl;
        return -1;
    }
    cout << "WSAStartup启动完成！" << endl;
    serverAddr.sin_family = AF_INET;//使用IPV4
    serverAddr.sin_port = htons(SERV_PORT);
    serverAddr.sin_addr.s_addr = htonl(2130706433);
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));//绑定套接字，进入监听状态
    cout << "套接字绑定完成！" << endl;
    cout << "进入监听状态..." << endl;.
    Connect();

    char* name = new char[20];
    char* file = new char[100000000];
    RecvMessage(name);
    int filelen = RecvMessage(file);
    disConnect();

    ofstream fout(name, ofstream::binary);
    for (int i = 0; i < filelen; i++) fout << file[i];
    fout.close();

    cout << "文件已下载到本地!" << endl;
    system("pause");
    return 0;
}
