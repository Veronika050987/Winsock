#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		//Только при подключении <Windows.h>
#endif // !WIN32_LEAN_AND_MEAN 


#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>
#include<iostream>
using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define DEFAULT_PORT	"27015"
#define BUFFER_LENGTH	1460

// Глобальные переменные для управления консолью
HANDLE hConsole;
CONSOLE_SCREEN_BUFFER_INFO csbi;
COORD origin = { 0, 0 };

// Функция для перемещения курсора в верхнюю часть консоли
void MoveToTop()
{
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    SetConsoleCursorPosition(hConsole, origin);
}

// Функция для очистки консоли
void ClearConsole()
{
    MoveToTop();
    DWORD written;
    FillConsoleOutputCharacterA(hConsole, ' ', csbi.dwSize.X * csbi.dwSize.Y, origin, &written);
    GetConsoleScreenBufferInfo(hConsole, &csbi); // Обновить информацию о буфере после очистки
}

// Функция для вывода сообщений в верхнюю часть консоли
void PrintMessage(const string& message)
{
    MoveToTop();
    // Очищаем строку, где будут выводиться новые сообщения
    DWORD written;
    FillConsoleOutputCharacterA(hConsole, ' ', csbi.dwSize.X, origin, &written);
    SetConsoleCursorPosition(hConsole, origin);
    cout << message << endl;
}

VOID Receive(SOCKET connect_socket);

int main()
{
    setlocale(LC_ALL, "");
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // Получаем хэндл для стандартного вывода

    INT iResult = 0;
    DWORD dwLastError = 0;

    //0) Инициализация WinSock:
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);	//MAKEWORD(2,2) - задает версию WinSock
    if (iResult != 0)
    {
        cout << "WSAStartup failed: " << iResult << endl;
        return iResult;
    }

    //1) Создаем переменные для хранения информации о сокете:
    addrinfo* result = NULL;
    addrinfo* ptr = NULL;
    addrinfo hints = { 0 };
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    //2) Задаем информацию о Сервере к которуму будем подключаться:
    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0)
    {
        cout << "getaddrinfo failed: " << iResult << endl;
        WSACleanup();
        return iResult;
    }

    //3) Создаем Cокет для подключения к Серверу:
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

    //4) Подключаемся к Серверу:
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

    // Создаем поток, который будет принимать сообщения от Сервера и выводить их на экран:
    DWORD dwThreadID = 0;
    HANDLE hRecvThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Receive,
        (LPVOID)connect_socket, 0, &dwThreadID);

    //5) Отправляем данные на Сервер:
    CHAR send_buffer[BUFFER_LENGTH] = "";
    do
    {
        ZeroMemory(send_buffer, BUFFER_LENGTH);
        // Перемещаем курсор вниз для ввода сообщения
        GetConsoleScreenBufferInfo(hConsole, &csbi); // Получаем текущие координаты курсора
        COORD input_cursor_pos = { 0, csbi.dwSize.Y - 1 }; // Позиция для ввода внизу
        SetConsoleCursorPosition(hConsole, input_cursor_pos);

        cout << "Введите сообщение: ";
        SetConsoleCP(1251); // Устанавливаем кодировку для ввода
        cin.getline(send_buffer, BUFFER_LENGTH);
        SetConsoleCP(866); // Возвращаем кодировку по умолчанию (или ту, что использовалась ранее)

        iResult = send(connect_socket, send_buffer, strlen(send_buffer), 0);
        if (iResult == SOCKET_ERROR)
        {
            dwLastError = WSAGetLastError();
            cout << "Send failed with error: " << dwLastError << endl;
            closesocket(connect_socket);
            freeaddrinfo(result);
            WSACleanup();
            return dwLastError;
        }
        // cout << iResult << " Bytes sent" << endl; // Удаляем или комментируем вывод количества отправленных байт, если он мешает

    } while (strstr(send_buffer, "exit") == 0 && strstr(send_buffer, "quit") == 0);

    CloseHandle(hRecvThread);

    //7)Отключение от Сервера:
    send(connect_socket, "quit", 4, 0);
    iResult = shutdown(connect_socket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        dwLastError = WSAGetLastError();
        cout << "Shutdown failed with error: " << dwLastError << endl;
    }
    closesocket(connect_socket);
    freeaddrinfo(result);
    WSACleanup();
    return dwLastError;
}

VOID Receive(SOCKET connect_socket)
{
    INT iResult = 0;
    CHAR recv_buffer[BUFFER_LENGTH] = {};
    do
    {
        iResult = recv(connect_socket, recv_buffer, BUFFER_LENGTH, 0);
        if (iResult > 0)
        {
            string received_message = string(recv_buffer, iResult);
            PrintMessage("Получено: " + received_message); // Используем функцию для вывода вверху
        }
        else if (iResult == 0) {
            PrintMessage("Соединение закрыто.");
        }
        else
        {
            DWORD error_code = WSAGetLastError();
            char error_buffer[20]; // Буфер достаточного размера для числа
            sprintf(error_buffer, "%lu", error_code); // Используем sprintf для форматирования
            PrintMessage("Ошибка получения: " + string(error_buffer));
        }
    } while (iResult > 0);
}