#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<SOCKET> clients;
mutex clientsMutex;

void removeClient(SOCKET s) {
    lock_guard<mutex> lock(clientsMutex);
    clients.erase(remove(clients.begin(), clients.end(), s), clients.end());
}

void broadcast(const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(clientsMutex);
    for (SOCKET client : clients) {
        if (client != sender) {
            send(client, msg.c_str(), (int)msg.length(), 0);
        }
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytes <= 0) {
            cout << "Client disconnected\n";
            removeClient(clientSocket);
            closesocket(clientSocket);
            break;
        }

        string msg(buffer);
        cout << "Received: " << msg << endl;

        broadcast(msg, clientSocket);
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "WSAStartup failed!\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed!\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(PORT));
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Bind failed!\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed!\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server started on port " << PORT << "...\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);

        if (clientSocket == INVALID_SOCKET) {
            cout << "Accept failed!\n";
            continue;
        }

        {
            lock_guard<mutex> lock(clientsMutex);
            clients.push_back(clientSocket);
        }
        cout << "Client connected!\n";

        thread t(handleClient, clientSocket);
        t.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
}
