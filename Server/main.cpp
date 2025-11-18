#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>
#include<iostream>
#include<string> // Для использования std::string
#include<vector> // Для использования std::vector
#include<algorithm> // Для std::find_if
using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT	"27015"
#define BUFFER_LENGTH	  1460
#define MAX_CLIENTS			 3
#define g_sz_SORRY			"Error: Количество подключений превышено"
#define IP_STR_MAX_LENGTH	16

INT g_activeClients = 0;	//Количество активных клиентов
SOCKET client_sockets[MAX_CLIENTS] = {};
DWORD threadIDs[MAX_CLIENTS] = {};
HANDLE hThreads[MAX_CLIENTS] = {};

HANDLE g_hMutex;//позволяет потокам, которые используют общие данные, безопасно 
//обмениваться данными между собой, предотвращая конфликты, когда несколько потоков 
// //пытаются получить доступ к общим переменным одновременно. 

void DisplayServerStatus()
{
	WaitForSingleObject(g_hMutex, INFINITE);

	cout << "\n-----------------------------------------" << endl;
	cout << "Сервер: Активных клиентов: " << g_activeClients << endl;
	cout << "Сервер: Свободных слотов: " << MAX_CLIENTS - g_activeClients << endl;
	cout << "-----------------------------------------" << endl;

	// Освобождаем мьютекс
	ReleaseMutex(g_hMutex);
}

VOID WINAPI HandleClient(LPVOID lpParam);

int main()
{
	setlocale(LC_ALL, "");

	DWORD dwLastError = 0;
	INT iResult = 0;

	// Инициализация мьютекса
	g_hMutex = CreateMutex(NULL, FALSE, NULL); // FALSE - мьютекс не владеет при создании
	if (g_hMutex == NULL) 
	{
		cout << "CreateMutex failed with error: " << GetLastError() << endl;
		return 1;
	}

	//0)Инициализируем WinSock:
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "WSA init failed with: " << dwLastError << endl;
		CloseHandle(g_hMutex); // Освобождаем мьютекс при выходе
		return dwLastError;
	}
	cout << "Winsock инициализирован." << endl;

	//1) Инициализируем переменные для сокета:
	addrinfo* result = NULL;
	addrinfo* ptr = NULL;
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;// Для использования в качестве прослушивающего сокета

	//2) Задаем параметры сокета:
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		dwLastError = WSAGetLastError();
		cout << "getaddrinfo failed with error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		CloseHandle(g_hMutex);
		return dwLastError;
	}
	cout << "Параметры сокета заданы." << endl;

	//3) Создаем сокет, который будет прослушивать Сервер:
	SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		dwLastError = WSAGetLastError();
		cout << "Socket creation failed with error: " << dwLastError << endl;
		freeaddrinfo(result);
		WSACleanup();
		CloseHandle(g_hMutex);
		return dwLastError;
	}
	cout << "Прослушивающий сокет создан." << endl;

	//4) Bind socket:
	iResult = bind(listen_socket, result->ai_addr, result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Bind failed with error: " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		CloseHandle(g_hMutex);
		return dwLastError;
	}
	cout << "Сокет привязан к порту " << DEFAULT_PORT << "." << endl;

	//5) Запускаем прослшивание сокета:
	iResult = listen(listen_socket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		dwLastError = WSAGetLastError();
		cout << "Listen failed with error: " << dwLastError << endl;
		closesocket(listen_socket);
		freeaddrinfo(result);
		WSACleanup();
		CloseHandle(g_hMutex);
		return dwLastError;
	}
	cout << "Сервер начал прослушивание." << endl;

	// Инициализируем массив client_sockets значениями INVALID_SOCKET, чтобы отметить свободные слоты
	for (int i = 0; i < MAX_CLIENTS; i++) 
	{
		client_sockets[i] = INVALID_SOCKET;
	}

	DisplayServerStatus();

	//6) Обработка запросов от клиентов:
	cout << "Accept client connections..." << endl;

	do
	{
		SOCKET client_socket = accept(listen_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET)
		{
			dwLastError = WSAGetLastError();
			cout << "Accept failed with error: " << dwLastError << endl;
			continue;
		}
		// Ждем, пока не получим доступ к мьютексу для работы с глобальными переменными
		WaitForSingleObject(g_hMutex, INFINITE);

		if (g_activeClients < MAX_CLIENTS)
		{
			// Находим первый свободный слот
			int free_slot_index = -1;
			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (client_sockets[i] == INVALID_SOCKET) // Проверяем, что слот действительно свободен
				{
					free_slot_index = i;
					break;
				}
			}

			if (free_slot_index != -1)
			{
				client_sockets[free_slot_index] = client_socket;
				g_activeClients++; // Увеличиваем счетчик активных клиентов

				// Создаем поток для обработки клиента
				hThreads[free_slot_index] = CreateThread(
					NULL,                   // default security attributes
					0,                      // default stack size
					(LPTHREAD_START_ROUTINE)HandleClient, // function to be executed
					(LPVOID)&client_sockets[free_slot_index],    // argument to the function
					0,                      // default creation flags
					&threadIDs[free_slot_index] // receives the thread identifier
				);

				if (hThreads[free_slot_index] == NULL)
				{
					dwLastError = GetLastError();
					cout << "CreateThread failed with error: " << dwLastError << endl;
					closesocket(client_socket); // Закрываем сокет, если поток не удалось создать
					client_sockets[free_slot_index] = INVALID_SOCKET; // Освобождаем слот
					g_activeClients--; // Уменьшаем счетчик активных клиентов
				}
				else
				{
					cout << "Новый клиент подключен. Обрабатывается в потоке " 
						<< threadIDs[free_slot_index] << endl;
				}
			}
			else
			{
				// Ситуация, когда g_activeClients < MAX_CLIENTS, но свободный слот не найден - не должна возникать при правильной логике.
				// Но на всякий случай обрабатываем.
				cout << "Ошибка: Не найден свободный слот, хотя g_activeClients < MAX_CLIENTS." << endl;
				closesocket(client_socket); // Закрываем соединение
			}
		}
		else
		{
			// Если максимальное количество клиентов уже достигнуто
			cout << "Максимальное количество клиентов (" << MAX_CLIENTS 
				<< ") достигнуто. Отклоняем новое подключение." << endl;
			// Отправляем сообщение о перегрузке
			INT iSendResult = send(client_socket, g_sz_SORRY, (int)strlen(g_sz_SORRY), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				cout << "Send failed with error: " << WSAGetLastError() << endl;
			}
			closesocket(client_socket); // Закрываем соединение
		}
		// Отображаем обновленное состояние сервера
		DisplayServerStatus();

		// Освобождаем мьютекс
		ReleaseMutex(g_hMutex);
	} while (true);

	closesocket(listen_socket);
	freeaddrinfo(result);
	WSACleanup();
	CloseHandle(g_hMutex); // Закрываем хэндл мьютекса
	return 0;
}

VOID WINAPI HandleClient(LPVOID lpParam)
{
	SOCKET client_socket = *(SOCKET*)lpParam;

	SOCKADDR_IN peer{};
	CHAR address[IP_STR_MAX_LENGTH] = {};
	INT address_length = sizeof(peer);

	// Получаем информацию о подключенном клиенте
	if (getpeername(client_socket, (SOCKADDR*)&peer, &address_length) == SOCKET_ERROR)
	{
		cout << "getpeername failed with error: " << WSAGetLastError() << endl;
	}
	else
	{
		// Преобразуем IP-адрес в строку
		if (inet_ntop(AF_INET, &peer.sin_addr, address, IP_STR_MAX_LENGTH) == NULL)
		{
			cout << "inet_ntop failed with error: " << WSAGetLastError() << endl;
			strcpy(address, "unknown"); // Если не удалось получить адрес
		}
		// Преобразуем порт в человекочитаемый формат (порядок байт)
		INT port = ntohs(peer.sin_port);

		cout << "Клиент подключен: " << address << ":" << port << endl;

		// 7) Получение запросов от клиента:
		CHAR recv_buffer[BUFFER_LENGTH] = {};
		INT iResult = 0;
		DWORD dwLastError = 0;

		do
		{
			ZeroMemory(recv_buffer, BUFFER_LENGTH);
			iResult = recv(client_socket, recv_buffer, BUFFER_LENGTH, 0);

			if (iResult > 0)
			{
				cout << "Получено " << iResult << " байт от " << address << ":" << port << ": " 
					<< recv_buffer << endl;

				// Эхо-ответ клиенту
				INT iSendResult = send(client_socket, recv_buffer, iResult, 0);
				if (iSendResult == SOCKET_ERROR)
				{
					dwLastError = WSAGetLastError();
					cout << "Send failed for client " << address << ":" << port << " with error: " 
						<< dwLastError << endl;
					break; // Прерываем цикл при ошибке отправки
				}
			}
			else if (iResult == 0)
			{
				cout << "Клиент " << address << ":" << port << " закрыл соединение." << endl;
				break; // Клиент отключился
			}
			else
			{
				dwLastError = WSAGetLastError();
				if (dwLastError != WSAECONNRESET && dwLastError != WSAETIMEDOUT && dwLastError != WSAEINTR) 
					// Игнорируем некоторые распространенные ошибки при отключении
				{
					cout << "Receive failed for client " << address << ":" << port << " with error: " 
						<< dwLastError << endl;
				}
				break; // Прерываем цикл при ошибке приема
			}
		} while (iResult > 0 && _stricmp(recv_buffer, "quit") != 0 && _stricmp(recv_buffer, "exit") != 0); 
		// Продолжаем, пока есть данные и клиент не ввел quit/exit

		// Клиент отключился или ввел команду выхода
		cout << "Клиент " << address << ":" << port << " отключился." << endl;

		// Находим индекс слота, который нужно освободить
		int slot_to_free = -1;
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (client_sockets[i] == client_socket)
			{
				slot_to_free = i;
				break;
			}
		}

		WaitForSingleObject(g_hMutex, INFINITE);

		if (slot_to_free != -1)
		{
			closesocket(client_sockets[slot_to_free]);
			client_sockets[slot_to_free] = INVALID_SOCKET; // Помечаем слот как свободный
			threadIDs[slot_to_free] = 0; // Обнуляем ID потока
			hThreads[slot_to_free] = NULL; // Обнуляем хэндл потока
			g_activeClients--; // Уменьшаем счетчик активных клиентов

			// Отображаем обновленное состояние сервера
			DisplayServerStatus();
		}
		ReleaseMutex(g_hMutex);
	}

	// Завершаем поток
	ExitThread(0);
}