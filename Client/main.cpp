#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include<Windows.h>
#include<WinSock2.h>
#include<ws2tcpip.h>
#include<iphlpapi.h>
#include<iostream>
#include<string> // Для использования std::string и std::cin
#include<limits> // Для std::numeric_limits
using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT "27015"
#define BUFFER_LENGTH 1460

int main()
{
	setlocale(LC_ALL, "");
	INT iResult = 0;
	DWORD dwLastError = 0;

	//0) Инициализация Winsock
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);//MAKEWORD(2, 2) эадаёт версию Winsock
	if (iResult != 0)
	{
		cout << "WSAStartup failed: " << iResult << endl;
		return iResult;
	}
	cout << "WinSock initialized\n" << endl;

	//1) Создаём переменную для хранения информации о сокете
	addrinfo* result = NULL;
	addrinfo* ptr = NULL;
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//2) Задаём информацию о сервере, к которому будем подключаться
	iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		cout << "getaddrinfo failed: " << iResult << endl;
		WSACleanup();
		return iResult;
	}
	cout << "Address info obtained\n" << endl;

	//3) Создаём сокет для подключения к серверу
	ptr = result;
	SOCKET connect_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (connect_socket == INVALID_SOCKET)
	{
		dwLastError = WSAGetLastError();
		cout << "Socket error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;

	}
	cout << "Connect socket created\n" << endl;

	//4) Подключаемся к серверу
	iResult = connect(connect_socket, ptr->ai_addr, (INT)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Connection error: " << dwLastError << endl;
		closesocket(connect_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}
	cout << "Connected to server\n" << endl;

	//5) Отправляем данные на сервер
	string message;
	const string exit_command_str = "exit"; // Для сравнения с введенным сообщением
	const string quit_command_str = "quit";

	do
	{
		cout << "Enter message (or 'exit'/'quit' to quit): ";
		// Читаем строку с клавиатуры
		getline(cin, message);

		if (message == exit_command_str || message == quit_command_str)
		{
			iResult = send(connect_socket, message.c_str(), message.length(), 0);
			if (iResult == SOCKET_ERROR)
			{
				dwLastError = WSAGetLastError();
				cout << "Send exit command failed with error: " << dwLastError << endl;
			}
			else
			{
				cout << "Sent exit command (" << iResult << " bytes)." << endl;
			}
			break;
		}

		// Отправляем сообщение на сервер
		iResult = send(connect_socket, message.c_str(), message.length(), 0);
		if (iResult == SOCKET_ERROR)
		{
			dwLastError = WSAGetLastError();
			cout << "Send failed with error: " << dwLastError << endl;
			break;
		}
		cout << iResult << " Bytes sent." << endl;

		// Ожидаем ответ от сервера
		CHAR recv_buffer[BUFFER_LENGTH] = {};
		iResult = recv(connect_socket, recv_buffer, BUFFER_LENGTH - 1, 0); // Оставляем место для null-терминатора
		if (iResult > 0)
		{
			recv_buffer[iResult] = '\0'; // Добавляем null-терминатор
			cout << "Received from server (" << iResult << " bytes): " << recv_buffer << endl;
		}
		else if (iResult == 0)
		{
			cout << "Server closed the connection." << endl;
			break; // Сервер отключился
		}
		else
		{
			dwLastError = WSAGetLastError();
			cout << "Receive failed with error: " << dwLastError << endl;
			break; // Ошибка при получении
		}

	} while (true); // Цикл будет прерван break'ом

	//6) Ожидаем ответ от сервера
	iResult = shutdown(connect_socket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Shutdown failed with error: " << dwLastError << endl;
	}
	else
	{
		cout << "Shutdown initiated\n" << endl;
	}

	//7) Отключение сервера
	closesocket(connect_socket);
	cout << "Connect socket closed\n" << endl;
	freeaddrinfo(result);
	WSACleanup();
	cout << "WinSock cleaned up\n" << endl;
	return 0;
}