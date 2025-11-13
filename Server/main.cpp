#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>
#include<iostream>
#include<string> // Для использования std::string и std::cin
#include<cstring> // Для использования strncmp
using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT "27015"
#define BUFFER_LENGTH 1460

int main()
{
	setlocale(LC_ALL, "");
	DWORD dwLastError = 0;
	INT iResult = 0;
	
	//0)Инициализируем WinSock
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "WSA init failed with: " << dwLastError << endl;
		return dwLastError;
	}
	cout << "WinSock initialized!\n" << endl;

	//1) Инициализируем переменную для сокета
	addrinfo* result = NULL;
	addrinfo* ptr = NULL;
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//2) Задаём параметры сокета
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "getaddrinfo failed with error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Address info obtained\n" << endl;

	//3) Создаём сокет, который будет прослушивать сервер
	SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		dwLastError = WSAGetLastError();
		cout << "Socket creation failed with error " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Listen socket created\n" << endl;

	//4) Bind socket
	iResult = bind(listen_socket, result->ai_addr, result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Bind failed with error " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Socket bound to port " << DEFAULT_PORT << "\n" << endl;

	//5) Запускаем прослушивание сокета
	iResult = listen(listen_socket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Listen failed with error " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Listening for incoming connections...\n" << endl;

	//6) Обработка запросов от клиента
	SOCKET client_socket = accept(listen_socket, NULL, NULL);
	if (client_socket == INVALID_SOCKET)
	{
		dwLastError = WSAGetLastError();
		cout << "Accept failed with error " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Client connected\n" << endl;

	//7) Получение и отправка сообщения от клиента
	CHAR recv_buffer[BUFFER_LENGTH] = {};
	const char* exit_commands[] = { "exit","quit" };
	bool client_disconnected = false;

	do
	{
		iResult = recv(client_socket, recv_buffer, BUFFER_LENGTH-1, 0);// Оставляем место для null-терминатора
		if (iResult > 0)
		{
			recv_buffer[iResult = '\0'];// Добавляем null-терминатор для работы со строками
			cout <<  "Received(" << iResult << "bytes):" << recv_buffer << endl;

			bool should_exit = false;
			for (const char* cmd : exit_commands)
			{
				if (strncmp(recv_buffer, cmd, strlen(cmd)) == 0)
				{
					should_exit = true;
					break;
				}
			}
			if (should_exit) 
			{
				cout << "Client requested to exit. Closing connection\n" << endl;
				// Отправляем подтверждение перед закрытием
				const char* response = "Server acknowledging exit. Goodbye!\n";
				send(client_socket, response, strlen(response), 0);
				client_disconnected = true;
				break; // Выходим из цикла приема/отправки
			}
			else 
			{
				// Отправляем ответ клиенту (эхо-сообщение)
				string response_str = "Server received: " + string(recv_buffer);
				iResult = send(client_socket, response_str.c_str(), response_str.length(), 0);
				if (iResult == SOCKET_ERROR)
				{
					dwLastError = WSAGetLastError();
					cout << "Send failed with error " << dwLastError << endl;
					client_disconnected = true; // Предполагаем, что клиент мог отключиться
					break;
				}
				cout << "Sent (" << iResult << " bytes): " << response_str << endl;
			}
		}
		else if (iResult == 0)
		{
			cout << "Client disconnected gracefully\n" << endl;
			client_disconnected = true;
		}
		else
		{
			dwLastError = WSAGetLastError();
			cout << "Receive failed with error " << dwLastError << endl;
			client_disconnected = true;
			break;
		}
		memset(recv_buffer, 0, BUFFER_LENGTH);// Сбрасываем буфер для следующего сообщения
	
	} while (!client_disconnected);

	closesocket(client_socket);
	cout << "Client socket closed\n" << endl;
	closesocket(listen_socket);
	cout << "Listen socket closed\n" << endl;
	freeaddrinfo(result);
	WSACleanup();
	cout << "WinSock cleaned up\n" << endl;
	return 0;
}