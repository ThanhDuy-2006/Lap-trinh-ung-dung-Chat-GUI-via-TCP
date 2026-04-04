#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Nhận tin nhắn từ server
void receiveMessages(SOCKET sock) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sock, buffer, sizeof(buffer), 0);

        if (bytes <= 0) {
            cout << "\nDisconnected from server\n";
            break;
        }

        cout << "\n>> " << buffer << endl;
        cout.flush();
    }
}

int main() {
    WSADATA wsa;

    // Khởi tạo Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "WSAStartup failed!\n";
        return 1;
    }

    // Tạo socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Socket creation failed!\n";
        WSACleanup();
        return 1;
    }

    // Cấu hình server (cùng PORT với Protocol.h / server)
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(PORT));
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Kết nối server
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connection failed!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!\n";

    // Thread nhận tin nhắn
    thread t(receiveMessages, sock);
    t.detach();

    string msg;

    // Gửi tin nhắn
    while (true) {
        getline(cin, msg);

        if (!msg.empty()) {
            int result = send(sock, msg.c_str(), (int)msg.length(), 0);

            if (result == SOCKET_ERROR) {
                cout << "Send failed!\n";
                break;
            }
        }
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
