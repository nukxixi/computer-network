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
const int BUF_SIZE = 1024;//���仺������󳤶�
const int RESEND_TIMES = 30;
const int HEADER_SIZE = 22;
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
        sum += temp[i];
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

int Connect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    //���յ�һ������
    while (1) {
        if (recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen) > 0) {
            ::memcpy(&msg, BUFFER, sizeof(msg));
            if (msg.getFlags() == SYN && msg.akCheckSum() == 1)
            {
                cout << "���յ���һ��������Ϣ~" << endl;
                break;
            }
        }
    }

    //���͵ڶ���������Ϣ
    msg.setFlags(ACK);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
    start = clock();//��¼�ڶ������ַ���ʱ��

    //���յ���������
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
            cout << "�ڶ������ֳ�ʱ�����ڽ����ش�..." << endl;
        }
    }

    //���յ�����������
    message msg1;
    ::memcpy(&msg1, BUFFER, sizeof(msg1));
    if (msg1.getFlags() == SYN_ACK && msg1.akCheckSum() == 1)
    {
        cout << "���������ֳɹ�~" << endl;
        cout << "�ɹ�����ͨ�ţ����Խ�������" << endl;
    }
    else
    {
        return -1;
    }
    return 1;
}

int RecvMessage(char* text)
{
    long int file = 0;//�ļ�����
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    int ack = 0;

    while (1)
    {
        recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen);//���ձ��ĳ���
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.akCheckSum() == 1) {
            if (msg.getFlags() == SEND_FILE)
            {
                //�жϽ��յ�����ȷ�İ���
                if (ack != int(msg.seq))
                {
                    //������ȷ�İ����ط���ǰ����ACK�������������ݰ�
                    msg.setFlags(ACK);
                    msg.filesize = 0;
                    msg.ack = (u_short)msg.seq;
                    msg.setCheckSum();
                    ::memcpy(BUFFER, &msg, sizeof(msg));
                    sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
                    cout << "�ɹ�����ȷ�����ݰ�~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
                    continue;
                }

                ack = int(msg.seq);
                ack %= 65535;

                //ȡ��BUFFER�е�����
                cout << "�ɹ����� " << int(msg.filesize) << " bytes ���ݣ� Flags:" << hex << "0x" << int(msg.getFlags()) << "(SEND_FILE)" << " Seq number:" << dec << int(msg.seq) << endl;
                ::memcpy(text + file, BUFFER + HEADER_SIZE, int(msg.filesize));
                text[file + msg.filesize] = '\0';
                file = file + int(msg.filesize);

                //����ACK��
                msg.setFlags(ACK);
                msg.filesize = 0;
                msg.ack = (u_short)ack;
                msg.setCheckSum();
                ::memcpy(BUFFER, &msg, sizeof(msg));
                sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen);
                cout << "�ɹ�����ȷ�����ݰ�~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
                ack++;
                ack %= 65535;
            }

            //�ǽ�����
            else if (msg.getFlags() == OVER_FILE)
            {
                cout << "������ļ����գ�" << endl;
                break;
            }
        }
    }

    //����OVER_FILE��ȷ�ϰ�
    msg.setFlags(ACK);
    msg.setCheckSum();
    msg.ack = (u_short)ack;
    ::memcpy(BUFFER, &msg, sizeof(msg));
    if (sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen) == -1)
    {
        return -1;
    }
    cout << "�ɹ�����ȷ�����ݰ�~  Flags:" << hex << "0x" << int(msg.getFlags()) << "(ACK)" << " Ack number:" << dec << int(msg.ack) << endl;
    return file;
}

int disConnect()
{
    message msg;
    char* BUFFER = new char[sizeof(msg)];
    clock_t start, end;
    while (1)
    {
        recvfrom(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, &clientAddrLen);//���ձ��ĳ���
        ::memcpy(&msg, BUFFER, sizeof(msg));
        if (msg.getFlags() == FIN && msg.akCheckSum() == 1)
        {
            cout << "�ɹ����յ�һ�λ�����Ϣ~" << endl;
            break;
        }
    }

    //���͵ڶ��λ���
    msg.setFlags(ACK);
    msg.setCheckSum();
    ::memcpy(BUFFER, &msg, sizeof(msg));
    if (sendto(serverSocket, BUFFER, sizeof(msg), 0, (sockaddr*)&clientAddr, clientAddrLen) == -1)
    {
        return -1;
    }
    cout << "���ӶϿ�!";
    return 1;
}


int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup����" << WSAGetLastError() << endl;
        return -1;
    }
    cout << "WSAStartup������ɣ�" << endl;
    serverAddr.sin_family = AF_INET;//ʹ��IPV4
    serverAddr.sin_port = htons(SERV_PORT);
    serverAddr.sin_addr.s_addr = htonl(2130706433);
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));//���׽��֣��������״̬
    cout << "�׽��ְ���ɣ�" << endl;
    cout << "�������״̬..." << endl;.
    Connect();

    char* name = new char[20];
    char* file = new char[100000000];
    RecvMessage(name);
    int filelen = RecvMessage(file);
    disConnect();

    ofstream fout(name, ofstream::binary);
    for (int i = 0; i < filelen; i++) fout << file[i];
    fout.close();

    cout << "�ļ������ص�����!" << endl;
    system("pause");
    return 0;
}
