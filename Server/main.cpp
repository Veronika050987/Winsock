#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <vector>   // Для динамического хранения
#include <string>   // Для работы со строками (например, для "exit")

using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT	"27015"
#define BUFFER_LENGTH	  1460
#define MAX_CLIENTS			 5 // Максимальное количество одновременно подключенных клиентов

// Структура для передачи данных в поток
struct ClientInfo 
{
    SOCKET client_socket;
    DWORD thread_id;
    HANDLE thread_handle;
};

// Функция, которая будет выполняться в отдельном потоке для каждого клиента
DWORD WINAPI HandleClient(LPVOID lpParam)
{
    ClientInfo* info = (ClientInfo*)lpParam;
    SOCKET client_socket = info->client_socket;

    INT iResult = 0;
    DWORD dwLastError = 0;

    cout << "Client connected on socket: " << client_socket << " (Thread ID: " 
        << info->thread_id << ")" << endl;

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
                cout << "Send failed for client " << client_socket << " with error: " 
                    << dwLastError << endl;
                break;
            }
            cout << "Sent " << iSendResult << " bytes back to client " << client_socket << endl;
        }
        else if (iResult == 0)
        {
            cout << "Client " << client_socket << " disconnected." << endl;
            break;
        }
        else
        {
            dwLastError = WSAGetLastError();
            cout << "Receive failed for client " << client_socket << " with error: " 
                << dwLastError << endl;
            break;
        }
    } while (iResult > 0);

    // Закрываем сокет клиента
    closesocket(client_socket);
    cout << "Connection for client " << client_socket << " closed." << endl;

    // Важно: Поток должен вернуть память, выделенную для ClientInfo, если она была выделена динамически.
    // В данном случае, мы передаем указатель на элемент вектора, который будет управлять памятью.
    // Сам поток завершает свою работу.
    return 0; // Завершаем поток
}

int main()
{
    setlocale(LC_ALL, "");

    DWORD dwLastError = 0;
    INT iResult = 0;

    // 0) Инициализация WinSock:
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        dwLastError = WSAGetLastError();
        cout << "WSAStartup failed with error: " << dwLastError << endl;
        return dwLastError;
    }

    // 1) Инициализация переменных для сокета:
    addrinfo* result = NULL;
    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_protocol = IPPROTO_TCP; // Протокол TCP
    hints.ai_flags = AI_PASSIVE;     // Используется для биндинга на локальном адресе

    // 2) Задаем параметры сокета:
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0)
    {
        dwLastError = WSAGetLastError();
        cout << "getaddrinfo failed with error: " << dwLastError << endl;
        freeaddrinfo(result);
        WSACleanup();
        return dwLastError;
    }

    // 3) Создаем сокет, который будет прослушивать Сервер:
    SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET)
    {
        dwLastError = WSAGetLastError();
        cout << "Socket creation failed with error: " << dwLastError << endl;
        freeaddrinfo(result);
        WSACleanup();
        return dwLastError;
    }

    // 4) Bind socket (привязываем сокет к адресу и порту):
    iResult = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        dwLastError = WSAGetLastError();
        cout << "Bind failed with error: " << dwLastError << endl;
        closesocket(listen_socket);
        freeaddrinfo(result);
        WSACleanup();
        return dwLastError;
    }

    // 5) Запускаем прослушивание сокета:
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

    // 6) Обработка запросов от клиентов:
    vector<ClientInfo> clients; // Динамический массив для хранения информации о клиентах
    bool server_running = true;

    do
    {
        // Принимаем новое подключение
        SOCKET client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET)
        {
            dwLastError = WSAGetLastError();
            cout << "Accept failed with error: " << dwLastError << endl;
            // В случае ошибки accept, можно либо завершить сервер, либо пропустить это подключение
            // Если ошибка критичная (например, 10004 - operation cancelled), то можем выйти.
            if (dwLastError == WSAEINTR) // Операция прервана (например, при закрытии сервера)
                server_running = false;
            else
                continue; // Пропускаем текущую итерацию и продолжаем ожидать
        }

        // Проверяем, не превышено ли максимальное количество клиентов
        if (clients.size() < MAX_CLIENTS)
        {
            // Создаем структуру для информации о клиенте
            ClientInfo new_client_info;
            new_client_info.client_socket = client_socket;

            // Создаем новый поток для обработки данного клиента
            // Передаем указатель на структуру new_client_info как параметр потока
            // ВАЖНО: new_client_info должна быть доступна потоку до его завершения.
            // Поэтому лучше добавить ее в вектор ДО создания потока.

            clients.push_back(new_client_info); // Добавляем информацию в вектор
            // Теперь new_client_info (копия) находится в clients.back()

            HANDLE hThread = CreateThread
            (
                NULL,                       // Атрибуты безопасности
                0,                          // Размер стека
                HandleClient,               // Функция потока
                &clients.back(),            // Параметр потока (указатель на элемент вектора)
                0,                          // Флаги создания
                &clients.back().thread_id   // Получаем ID потока
            );

            if (hThread == NULL)
            {
                dwLastError = GetLastError();
                cout << "CreateThread failed with error: " << dwLastError << endl;
                // Если не удалось создать поток, удаляем информацию о клиенте из вектора
                closesocket(client_socket); // Закрываем сокет
                clients.pop_back();         // Удаляем из вектора
            }
            else
            {
                clients.back().thread_handle = hThread; // Сохраняем хэндл потока
                cout << "Client " << client_socket << " connected. Thread ID: " 
                    << clients.back().thread_id << endl;
            }
        }
        else
        {
            cout << "Max clients reached (" << MAX_CLIENTS << "). Rejecting connection from " 
                << client_socket << endl;
            closesocket(client_socket);
        }

    } while (server_running); // Продолжаем, пока сервер должен работать

    // --- Очистка ресурсов при завершении сервера ---
    cout << "Server shutting down..." << endl;
    for (size_t i = 0; i < clients.size(); ++i)
    {
        // Ждем завершения каждого клиентского потока
        if (clients[i].thread_handle != NULL)
        {
            WaitForSingleObject(clients[i].thread_handle, INFINITE); // Ждем бесконечно
            CloseHandle(clients[i].thread_handle); // Закрываем хэндл потока
        }
        // Сокет клиента уже должен быть закрыт в HandleClient, но на всякий случай:
        // closesocket(clients[i].client_socket); // (Если HandleClient не закрыл)
    }
    clients.clear(); // Очищаем вектор

    closesocket(listen_socket);
    freeaddrinfo(result);
    WSACleanup();
    cout << "Server shut down successfully." << endl;
    return 0;
}