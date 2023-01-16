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
const int BUF_SIZE = 1024;//���仺������󳤶�
const int RESEND_TIMES = 30;//����ش�����
const unsigned char SYN = 0x1;
const unsigned char ACK = 0x2;
const unsigned char SYN_ACK = 0x3;
const unsigned char FIN = 0x4;
const unsigned char SEND_FILE = 0x8;//�����ļ�
const unsigned char OVER_FILE = 0x16;//������־

struct message
{
#pragma pack(1)
    DWORD sendIp; //32λ������ip 
    DWORD recvIp; //32λ������ip 
    u_short sendPort; //16λ�����Ͷ˿ں� 
    u_short recvPort; //16λ�����ն˿ں� 
    u_short filesize = 0;//16λ�����ݲ��ֵĳ��� 
    u_short flags = 0;//16λ
    u_short seq = 0; //16λ����������кţ�0~65535
    u_short ack = 0; //16λ������ȷ�ϵ����к�
    u_short checksum = 0;//16λ,У���
    char msg[BUF_SIZE];//��������
#pragma pack()
    message() {
        checksum = 0;//У��� 16λ
        filesize = 0;//���������ݳ��� 16λ
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


//����У���
u_short message::setCheckSum() {
    int sum = 0;
    u_short* temp = (u_short*)this; 
    for (int i = 0; i < 10; i++) {//ÿ16bitһ�飬ȡǰ10�����
        sum += temp[i];
        while (sum >= 0x10000)//�������
        {
            int a = sum >> 16; //���λ
            sum += a;
        }
    }
    this->checksum = ~(u_short)sum; //��λȡ��
    return this->checksum;
}

//����У���
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
    //�������sum��У�����ӣ�����0xffff��˵��У��ɹ�
    if ((u_short)this->checksum + (u_short)sum == 65535)
        return 1;
    return -1;
}

int Connect()//�������֣���������
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;

    //���͵�һ����������
    msg.setFlags(SYN); 
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));//�����ݰ����뻺����
    if (sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1)
    {
        return -1;
    }
    //���ͳɹ�
    start = clock(); //��һ�����ֵķ���ʱ��
    cout << "�ɹ����͵�һ����������" << endl;

    //�׽ӿڵķ�����ģʽ
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    //���յڶ�������
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
            cout << "��һ�����ֳ�ʱ�����ڽ����ش�..." << endl;
            resend_count++;
        }
    }
    while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) > 0) {
        ::memcpy(&msg, BUFFER, sizeof(msg)); //�ڻ������е����ݷ������ݰ���
        if (msg.getFlags() == ACK && msg.akCheckSum() == 1)
        {
            cout << "�յ��ڶ���������Ϣ~" << endl;
            break;
        }
    }

    //���е���������
    msg.setFlags(SYN_ACK);
    msg.checksum = 0;
    msg.setCheckSum();
    if (sendto(clientSocket, (char*)&msg, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen) == -1)
    {
        return -1;
    }
    cout << "�ɹ�����ͨ�ţ�������server���������ˣ�" << endl;
    return 1;
}

void sendPkt(char* text, int len, int& order) //�������ݰ�
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    msg.filesize = len;
    msg.seq = u_short(order); //���ݰ������к�
    msg.setFlags(SEND_FILE);
    msg.setCheckSum();
    for (int i = 0; i < len; i++) {  //�����ݴ��뱨����
        msg.msg[i] = text[i];
    }
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen); //�������ݰ�
    cout << "�ɹ����� " << len << " bytes����!" << "   Flags:" <<hex<<"0x"<<int(msg.getFlags())<<"(SEND_FILE)" << " Seq number:"<<dec << int(msg.seq) << endl;
    start = clock();//��¼����ʱ��
    
    int resend_count = 0;
    while (1)  //����server�˵�ACK��
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
                msg.seq = u_short(order); //���ݰ������к�
                msg.setFlags(SEND_FILE); 
                msg.setCheckSum();
                for (int i = 0; i < len; i++) {  //�����ݴ��뱨����
                    msg.msg[i] = text[i];
                }
                ::memcpy(BUFFER, &msg, sizeof(msg));
                sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);//����
                cout << "δ�յ�server��ȷ��~ �ش����ݰ���... ";
                cout << "�ɹ����� " << len << " bytes����!" << "   Flags:" << hex << "0x" << int(msg.getFlags()) << "(SEND_FILE)" << " Seq number:" << dec << int(msg.seq)  << endl;
                clock_t start = clock();
                resend_count++;
            }
        }

        //�յ���server��ȷ�ϰ�
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.ack == u_short(order) && msg.getFlags() == ACK&&msg.akCheckSum()==1)
        {
            cout << "���յ��Է���ȷ��~ " << "   Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
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
    //�����ݱ���Ƭ
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

    //���ͽ�����Ϣ
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    msg.setFlags(OVER_FILE); 
    msg.seq = order % 65535;
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
    cout << "�����ѳɹ����ͣ�" << endl;
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
                cout << "δ�յ�server��ȷ��~ �ش�OVER_FILE����... ";
                start = clock();
                resend_count++;
            }
        }
        ::memcpy(&msg, BUFFER, sizeof(msg));//���������յ���Ϣ����ȡ
        if (msg.getFlags() == ACK&&msg.ack== u_short(order))
        {
            cout << "�Է��ѳɹ������ļ�!" << endl;
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(clientSocket, FIONBIO, &mode);//�Ļ�����ģʽ
}

int disConnect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;

    //���е�һ�λ���
    msg.setFlags(FIN);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));//���ײ����뻺����
    sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
    start = clock(); //��¼���͵�һ�λ���ʱ��

    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    //���յڶ��λ���
    int resend_count = 0;
    while (recvfrom(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, &serverAddrlen) <= 0)
    {
        if (resend_count > RESEND_TIMES)return -1;
        end = clock();
        if ((end - start) / CLOCKS_PER_SEC > WAIT_TIME) //��ʱ���ش���һ�λ���
        {
            msg.setFlags(FIN);
            msg.setCheckSum();
            ::memcpy(BUFFER, &msg, sizeof(msg));//���ײ����뻺����
            sendto(clientSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&serverAddr, serverAddrlen);
            start = clock();
            resend_count++;
            cout << "��һ�λ��ֳ�ʱ�����ڽ����ش�..." << endl;
        }
    }

    ::memcpy(&msg, BUFFER, sizeof(msg));
    if (msg.getFlags() == ACK && msg.akCheckSum() == 1)
    {
        cout << "�յ��ڶ��λ�����Ϣ!" << endl;
        cout << "���ӶϿ���" << endl;
        return 1;
    }
    else
    {
        cout << "���Ӵ����˳�����" << endl;
        return -1;
    }
}

int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    serverAddr.sin_family = AF_INET;//ʹ��IPV4
    serverAddr.sin_port = htons(SERV_PORT);
    serverAddr.sin_addr.s_addr = htonl(2130706433);
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (Connect() == -1) return 0;
    cout << "������Ҫ���͵��ļ����ƣ�" << endl;

    char* name = new char[20];
    cin.getline(name, 20);
    ifstream fin(name, ifstream::binary); //�����Ʒ�ʽ���ļ�
    char* BUFFER = new char[1000000000];
    int m = 0;
    while (!fin.eof()) {//���ֽڶ�ȡ�������ļ�
        BUFFER[m++] = fin.get();
    }

    SendMessage(name, strlen(name));
    SendMessage(BUFFER, m);
    disConnect();
    system("pause");
    return 0;
}


