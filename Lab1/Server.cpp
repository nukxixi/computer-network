#include <iostream>  
#include <string>  
#include <cstring>  
#include <vector>  
#include <iterator>  
#include <algorithm>  
#include <Winsock2.h>  
#include <Windows.h>  

using namespace std;
#pragma comment(lib,"ws2_32.lib")   //����socket��̬���ӿ�

int PORT;//�������˿ں�
SOCKET clientSocket; //�û�socket
vector <SOCKET> clientSocketGroup;  //�û�socket����
SYSTEMTIME st = { 0 }; //ϵͳʱ��

void timeShow(SYSTEMTIME st) //��ʾʱ�亯��
{
	GetLocalTime(&st);
	cout << st.wMonth << "��" << st.wDay << "�� " << st.wHour << ":";
	if (st.wMinute < 10) cout << 0;
	cout << st.wMinute << ":";
	if (st.wSecond < 10) cout << 0;
	cout << st.wSecond;
}

int main()
{
	timeShow(st);
	cout << "[ INFO    ] Start Server Manager.." << endl;
	WSADATA wsaData;//��ȡ�汾��Ϣ
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
	{ //����SOCKET�⣬ʧ���˳�
		timeShow(st);
		cout << "[ INFO    ] WSAStartup Failed." << endl;
		return -1;
	}
	timeShow(st);
	cout << "[ OK      ] WSAStartup Completed." << endl;
	timeShow(st);
	cout << "[ INFO    ] Server IP Address: 127.0.0.1" << endl;
	timeShow(st);
	cout << "[ GET     ] Please input the port for connection: ";
	cin >> PORT;

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);//����SOCKET(��ַ���ͣ��������ͣ�Э�飩
	timeShow(st);
	cout << "[ OK      ] Socket created sussessfully!" << endl;

	//����ַ�󶨵�SOCKET
	SOCKADDR_IN serverAddr; //sockaddr_in�׽��ֽṹ��
	serverAddr.sin_family = AF_INET; //Э��أ�TCP/IP��
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//��������ַ
	serverAddr.sin_port = htons(PORT);//�˿ں�
	bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));//�󶨵�ָ����socket
	timeShow(st);
	cout << "[ OK      ] Bind success!" << endl;

	//��ʼ����ģʽ
	if (listen(serverSocket, 20) == 0) 
	{
		timeShow(st);
		cout << "[ INFO    ] Start listening..." << endl;
	}
	else 
	{
		timeShow(st);
		cout << "[ INFO    ] Listen Failed." << endl;
		return -1;
	}

	//�̺߳��������
	DWORD WINAPI sendThread(LPVOID IpParameter);
	DWORD WINAPI receiveThread(LPVOID IpParameter);

	HANDLE sThread = CreateThread(NULL, 0, sendThread, NULL, 0, NULL);//���������̣߳�ֻ��һ��
	while (1)
	{
		clientSocket = accept(serverSocket, NULL, NULL);
		if (SOCKET_ERROR != clientSocket)
		{
			clientSocketGroup.push_back(clientSocket);
		}
		HANDLE rThread = CreateThread(NULL, 0, receiveThread, (LPVOID)clientSocket, 0, NULL);
	}
	CloseHandle(sThread); //�رշ��ͽ���
	closesocket(serverSocket);
	WSACleanup();   // ��ֹ���׽��ֿ��ʹ��  
}

DWORD WINAPI sendThread(LPVOID IpParameter) //�����߳�
{
	while (1)
	{
		string msg;
		getline(cin, msg);
		msg = "SERVER SAYS: " + msg;
		msg.append(" ");
		for (vector<SOCKET>::iterator it = clientSocketGroup.begin(); it != clientSocketGroup.end(); it++)
		{
			send(*it, msg.c_str(), 200, 0);
		}
	}
	return 0;
}

DWORD WINAPI receiveThread(LPVOID IpParameter) //�����߳�
{
	SOCKET ClientSocket = (SOCKET)(LPVOID)IpParameter;
	while (1)
	{
		char buf[300];
		recv(ClientSocket, buf, 200, 0);
		int len = strlen(buf);
		if (len >= 6 && buf[len - 6] == ':' && buf[len - 5] == 'q' && buf[len - 4] == 'u' && buf[len - 3] == 'i' && buf[len - 2] == 't' && buf[len - 1] == ' ')
		{
			//�û��˳�������
			vector<SOCKET>::iterator outClient = find(clientSocketGroup.begin(), clientSocketGroup.end(), ClientSocket);
			clientSocketGroup.erase(outClient);
			closesocket(ClientSocket);
			timeShow(st);
			cout << "[ INFO    ] Client ";
			string str_quit = "";
			str_quit.append(buf, strlen(buf) - 6);
			str_quit.append(" left room.");
			cout << str_quit << endl;
			for (vector<SOCKET>::iterator it = clientSocketGroup.begin(); it != clientSocketGroup.end(); it++)
			{
				send(*it, str_quit.c_str(), 200, 0);
			}
			break;
		}
		else if (buf[len - 1] == ' ' && buf[len - 2] == ':')
		{
			//�û�����������
			GetLocalTime(&st);
			timeShow(st);
			cout << "[ INFO    ] Client ";
			string str_enter = "";
			int m = 0;
			str_enter.append(buf, strlen(buf) - 2);
			str_enter.append(" entered room.");
			cout << str_enter << endl;
			for (vector<SOCKET>::iterator it = clientSocketGroup.begin(); it != clientSocketGroup.end(); it++)
			{
				send(*it, str_enter.c_str(), 200, 0);
			}
		}
		else
		{
			timeShow(st);
			cout << "[ MESSAGE ] ";
			for (int i = 0; i < strlen(buf); i++)
			{
				cout << buf[i];
			}
			cout << endl;
			for (vector<SOCKET>::iterator it = clientSocketGroup.begin(); it != clientSocketGroup.end(); it++)
			{
				send(*it, buf, 200, 0);
			}
		}
	}
	return 0;
}

