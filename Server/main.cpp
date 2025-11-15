#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>
#include<iostream>
#include <vector> // Для хранения сокетов клиентов

using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT	"27015"
#define BUFFER_LENGTH	  1460
#define MAX_CLIENTS			 5

DWORD WINAPI HandleClient(LPVOID lpParam)// Принимает указатель на сокет клиента
{
	SOCKET client_socket = *(SOCKET*)lpParam; // Получаем дескриптор сокета клиента
	INT iResult = 0;
	DWORD dwLastError = 0;

	cout << "Client connected on socket: " << client_socket << endl;

	// 7) Получение запросов от клиента:
	do
	{
		CHAR recv_buffer[BUFFER_LENGTH] = {};
		iResult = recv(client_socket, recv_buffer, BUFFER_LENGTH, 0);

		if (iResult > 0)
		{
			cout << "Received " << iResult << " bytes from client " << client_socket << ": "
				<< recv_buffer << endl;

			// Эхо-ответ клиенту
			INT iSendResult = send(client_socket, recv_buffer, iResult, 0);
			if (iSendResult == SOCKET_ERROR)
			{
				dwLastError = WSAGetLastError();
				cout << "Send failed for client " << client_socket << " (Thread ID: " 
					<< GetCurrentThreadId() << ") with error: " << dwLastError << endl;
				break; // Прерываем цикл при ошибке отправки
			}
			cout << "Sent " << iSendResult << " bytes back to client " << client_socket << endl;
		}
		else if (iResult == 0)
		{
			cout << "Client " << client_socket << " disconnected." << endl;
			break; // Клиент закрыл соединение
		}
		else
		{
			dwLastError = WSAGetLastError();
			cout << "Receive failed for client " << client_socket << " (Thread ID: " 
				<< GetCurrentThreadId() << ") with error: " << dwLastError << endl;
			break; // Прерываем цикл при ошибке приема
		}
	} while (iResult > 0); // Продолжаем, пока есть данные и соединение активно

	closesocket(client_socket);
	cout << "Connection for client " << client_socket << " closed." << endl;
	return 0;
}

int main()
{
	setlocale(LC_ALL, "");

	DWORD dwLastError = 0;
	INT iResult = 0;

	//0)Инициализируем WinSock:
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "WSA init failed with: " << dwLastError << endl;
		return dwLastError;
	}

	//1) Инициализируем переменные для сокета:
	addrinfo* result = NULL;
	addrinfo* ptr = NULL;
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//2) Задаем параметры сокета:
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "getaddrinfo failed with error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}

	//3) Создаем сокет, который будет прослушивать Сервер:
	SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		dwLastError = WSAGetLastError();
		cout << "Socket creation failed with error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}

	//4) Bind socket:
	iResult = bind(listen_socket, result->ai_addr, result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Bind failed with error: " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}

	//5) Запускаем прослшивание сокета:
	// SOMAXCONN - максимальное количество ожидающих соединений в очереди
	iResult = listen(listen_socket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Listen failed with error: " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		return dwLastError;
	}

	cout << "Server listening on port " << DEFAULT_PORT << "..." << endl;

	//6) Обработка запросов от клиентов:
	vector<SOCKET> client_sockets; // Динамический массив для хранения сокетов клиентов
	vector<HANDLE> hThreads;       // Динамический массив для хранения хэндлов потоков
	vector<DWORD> threadIDs;       // Динамический массив для хранения ID потоков

	do
	{
		// Принимаем новое подключение
		SOCKET client_socket = accept(listen_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET)
		{
			dwLastError = WSAGetLastError();
			cout << "Accept failed with error: " << dwLastError << endl;
			continue;
	}

		if (client_sockets.size() < MAX_CLIENTS)
		{
			// Добавляем сокет клиента в вектор
			client_sockets.push_back(client_socket);

			// Создаем новый поток для обработки данного клиента
			// Передаем указатель на сокет клиента как параметр потока
			HANDLE hThread = CreateThread(
				NULL,                   // Атрибуты безопасности (NULL - стандартные)
				0,                      // Размер стека (0 - по умолчанию)
				HandleClient,           // Адрес функции, выполняемой потоком
				&client_sockets.back(), // Параметр, передаваемый в функцию потока (указатель на последний добавленный сокет)
				0,                      // Флаги создания (0 - поток запускается немедленно)
				&threadIDs.back());     // Получаем ID потока

			if (hThread == NULL)
			{
				dwLastError = GetLastError();
				cout << "CreateThread failed with error: " << dwLastError << endl;
				closesocket(client_socket);
				client_sockets.pop_back();
			}
			else
			{
				// Добавляем хэндл потока в вектор
				hThreads.push_back(hThread);
				cout << "Client " << client_socket << " connected. Thread ID: " << 
					threadIDs.back() << endl;
			}
		}
		else
		{
			// Если достигнут лимит клиентов, отклоняем новое подключение
			cout << "Max clients reached. Rejecting connection from " << client_socket << endl;
			closesocket(client_socket);
		}

	} while (true); // Бесконечный цикл для приема новых подключений

	// Очистка ресурсов
	for (size_t i = 0; i < hThreads.size(); ++i)
	{
		CloseHandle(hThreads[i]);
	}
	for (size_t i = 0; i < client_sockets.size(); ++i)
	{
		closesocket(client_sockets[i]);
	}

	closesocket(listen_socket);
	freeaddrinfo(result);
	WSACleanup();
	return 0;
}
