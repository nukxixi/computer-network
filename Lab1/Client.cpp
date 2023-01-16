#include <iostream>  
#include <cstdio>  
#include <string>  
#include <cstring>  
#include <winsock2.h>  
#include <Windows.h>  
using namespace std;

SOCKET clientSocket;
string usrname;
string str;
int PORT;
string serverIp;
SYSTEMTIME st = { 0 };

#pragma comment(lib, "ws2_32.lib")

void timeShow(SYSTEMTIME st) //显示时间函数
{
	GetLocalTime(&st);
	cout << st.wMonth << "月" << st.wDay << "日 " << st.wHour << ":";
	if (st.wMinute < 10) cout << 0;
	cout << st.wMinute << ":";
	if (st.wSecond < 10) cout << 0;
	cout << st.wSecond;
}
int main()
{
	WSADATA wsaData;    // 用于接收Windows Socket的结构信息
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		timeShow(st);
		cout << "  WSAStartup Failed." << endl;
		return -1;
	}
	timeShow(st);
	cout << "  Please input Server IP Address: ";
	cin >> serverIp;
	timeShow(st);
	cout << "  Please input port for connection: ";
	cin >> PORT;
	timeShow(st);
	cout << "  Please input your name: ";
	cin >> usrname;
	str = usrname + ":";

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);//创建SOCKET，与远程主机相连
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET; //协议簇（TCP/IP）
	serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIp.c_str());//服务器地址
	serverAddr.sin_port = htons(PORT);//端口号
	connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));//连接服务器
	timeShow(st);
	cout << "  Connecting..." << endl;
	timeShow(st);
	cout << "  Connected success!" << endl;
	cout << "----------------------------------------------------------------------------------------------------------------" << endl;
	timeShow(st);
	cout << "  Welcome to the Chat Room!" << endl;

	DWORD WINAPI receiveThread(LPVOID IpParameter);
	DWORD WINAPI sendThread(LPVOID IpParameter);
	HANDLE sThread = CreateThread(NULL, 0, sendThread, NULL, 0, NULL);
	HANDLE rThread = CreateThread(NULL, 0, receiveThread, NULL, 0, NULL);

	//WaitForSingleObject(sThread, INFINITE);  // 等待线程结束  
	CloseHandle(rThread);
	CloseHandle(sThread); //关闭线程
	closesocket(clientSocket);
	WSACleanup();   // 终止对套接字库的使用  
}

DWORD WINAPI receiveThread(LPVOID IpParameter) //接收线程
{
	while (1)
	{
		char recvBuf[300];
		recv(clientSocket, recvBuf, 200, 0);
		timeShow(st);
		cout << "  ";
		for (int i = 0; i < strlen(recvBuf); i++)
		{
			cout << recvBuf[i];
		}
		cout << endl;
	}
	return 0;
}

DWORD WINAPI sendThread(LPVOID IpParameter) //发送线程
{
	while (1)
	{
		string msg;
		getline(cin, msg);
		msg = str + msg;
		if (msg == str + "quit")
		{
			msg.append(" ");
			send(clientSocket, msg.c_str(), 200, 0);
			return 0;
		}
		else
		{
			msg.append(" ");
			send(clientSocket, msg.c_str(), 200, 0);
			timeShow(st);
			cout << "  ";
			cout << "Message sent success!" << endl;
		}
	}
	return 0;
}