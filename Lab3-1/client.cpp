#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

SOCKADDR_IN serverAddr, clientAddr;
SOCKET clientSocket;
int serverAddrlen = sizeof(serverAddr);
const int SERV_PORT = 8000;
const int CLIT_PORT = 8100;
const int WAIT_TIME = 500;
const int BUF_SIZE = 1024;//传输缓冲区最大长度
const int RESEND_TIMES = 30;//最大重传次数
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
        checksum = 0;//校验和 16位
        filesize = 0;//所包含数据长度 16位
        flags = 0;
        seq = 0;
        ack = 0;
        sendPort = CLIT_PORT;
        recvPort = SERV_PORT;
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
        sum += temp[ i] ;
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

int Connect()//三次握手，建立连接
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;

    //发送第一次握手请求
    msg.setFlags(SYN); 
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));//将数据包放入缓冲区
    if (sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1)
    {
        return -1;
    }
    //发送成功
    start = clock(); //第一次握手的发送时间
    cout << "成功发送第一次握手请求！" << endl;

    //套接口的非阻塞模式
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    //接收第二次握手
    int resend_count = 0;
    while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) <= 0)
    {
        if (resend_count > RESEND_TIMES) return -1;
        end = clock();
        if ((end - start) / CLOCKS_PER_SEC > WAIT_TIME)
        {
            msg.setFlags(SYN); 
            msg.setCheckSum();
            ::memcpy(BUFFER, &msg, sizeof(msg));
            sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
            start = clock();
            cout << "第一次握手超时，正在进行重传..." << endl;
            resend_count++;
        }
    }
    while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) > 0) {
        ::memcpy(&msg, BUFFER, sizeof(msg)); //在缓冲区中的数据放入数据包中
        if (msg.getFlags() == ACK && msg.akCheckSum() == 1)
        {
            cout << "收到第二次握手信息~" << endl;
            break;
        }
    }

    //进行第三次握手
    msg.setFlags(SYN_ACK);
    msg.checksum = 0;
    msg.setCheckSum();
    if (sendto(clientSocket, (char*)&msg, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1)
    {
        return -1;
    }
    cout << "成功建立通信，可以向server发送数据了！" << endl;
    return 1;
}

void sendPkt(char* text, int len, int& order) //发送数据包
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    msg.filesize = len;
    msg.seq = u_short(order); //数据包的序列号
    msg.setFlags(SEND_FILE);
    msg.setCheckSum();
    for (int i = 0; i < len; i++) {  //将数据存入报文中
        msg.msg[i] = text[i];
    }
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen); //发送数据包
    cout << "成功发送 " << len << " bytes数据!" << "   Flags:" <<hex<<"0x"<<int(msg.getFlags())<<"(SEND_FILE)" << " Seq number:"<<dec << int(msg.seq) << endl;
    start = clock();//记录发送时间
    
    int resend_count = 0;
    while (1)  //接收server端的ACK包
    {
        u_long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);
        int resend_count = 0;
        while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) <= 0)
        {
            if (resend_count > RESEND_TIMES)return ;
            end = clock();
            if ((end - start) / CLOCKS_PER_SEC > WAIT_TIME) 
            {
                msg.filesize = len;
                msg.seq = u_short(order); //数据包的序列号
                msg.setFlags(SEND_FILE); 
                msg.setCheckSum();
                for (int i = 0; i < len; i++) {  //将数据存入报文中
                    msg.msg[i] = text[i];
                }
                ::memcpy(BUFFER, &msg, sizeof(msg));
                sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);//发送
                cout << "未收到server的确认~ 重传数据包中... ";
                cout << "成功发送 " << len << " bytes数据!" << "   Flags:" << hex << "0x" << int(msg.getFlags()) << "(SEND_FILE)" << " Seq number:" << dec << int(msg.seq)  << endl;
                clock_t start = clock();
                resend_count++;
            }
        }

        //收到了server的确认包
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.ack == u_short(order) && msg.getFlags() == ACK&&msg.akCheckSum()==1)
        {
            cout << "接收到对方的确认~ " << "   Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(clientSocket, FIONBIO, &mode);  
}

void SendMessage(char* text, int len)
{
    //将数据报分片
    int pkt_count = len / BUF_SIZE;
    pkt_count += (len % BUF_SIZE != 0);
    int order = 0;
    for (int i = 0; i < pkt_count; i++)
    {
        if (i == pkt_count - 1) {
            sendPkt(text + i * BUF_SIZE, len - i * BUF_SIZE, order);
            order++;
        }
        else {
            sendPkt(text + i * BUF_SIZE, BUF_SIZE, order);
            order++;
        }
        order %= 65535;
    }

    //发送结束信息
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    msg.setFlags(OVER_FILE); 
    msg.seq = order % 65535;
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
    cout << "数据已成功发送！" << endl;
    start = clock();
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);
        int resend_count = 0;
        while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) <= 0)
        {
            if (resend_count > RESEND_TIMES)return ;
            end = clock();
            if ((end - start) / CLOCKS_PER_SEC > WAIT_TIME) 
            {
                char* BUFFER = new char[sizeof(msg)];
                msg.setFlags(OVER_FILE); 
                msg.setCheckSum();
                msg.seq = order % 65535;
                ::memcpy(BUFFER, &msg, sizeof(msg));
                sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
                cout << "未收到server的确认~ 重传OVER_FILE包中... ";
                start = clock();
                resend_count++;
            }
        }
        ::memcpy(&msg, BUFFER, sizeof(msg));//缓冲区接收到信息，读取
        if (msg.getFlags() == ACK&&msg.ack== u_short(order))
        {
            cout << "对方已成功接收文件!" << endl;
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(clientSocket, FIONBIO, &mode);//改回阻塞模式
}

int disConnect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;

    //进行第一次挥手
    msg.setFlags(FIN);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));//将首部放入缓冲区
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
    start = clock(); //记录发送第一次挥手时间

    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    //接收第二次挥手
    int resend_count = 0;
    while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) <= 0)
    {
        if (resend_count > RESEND_TIMES)return -1;
        end = clock();
        if ((end - start) / CLOCKS_PER_SEC > WAIT_TIME) //超时，重传第一次挥手
        {
            msg.setFlags(FIN);
            msg.setCheckSum();
            ::memcpy(BUFFER, &msg, sizeof(msg));//将首部放入缓冲区
            sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
            start = clock();
            resend_count++;
            cout << "第一次挥手超时，正在进行重传..." << endl;
        }
    }

    ::memcpy(&msg, BUFFER, sizeof(msg));
    if (msg.getFlags() == ACK && msg.akCheckSum() == 1)
    {
        cout << "收到第二次挥手信息!" << endl;
        cout << "连接断开！" << endl;
        return 1;
    }
    else
    {
        cout << "连接错误，退出程序" << endl;
        return -1;
    }
}

int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    serverAddr.sin_family = AF_INET;//使用IPV4
    serverAddr.sin_port = htons(SERV_PORT);
    serverAddr.sin_addr.s_addr = htonl(2130706433);
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (Connect() == -1) return 0;
    cout << "请输入要发送的文件名称：" << endl;

    char* name = new char[20];
    cin.getline(name, 20);
    ifstream fin(name, ifstream::binary); //二进制方式打开文件
    char* BUFFER = new char[1000000000];
    int m = 0;
    while (!fin.eof()) {//按字节读取二进制文件
        BUFFER[m++] = fin.get();
    }

    SendMessage(name, strlen(name));
    SendMessage(BUFFER, m);
    disConnect();
    system("pause");
    return 0;
}


