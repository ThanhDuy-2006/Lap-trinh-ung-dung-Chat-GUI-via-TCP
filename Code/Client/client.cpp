#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool startsWith(const string& text, const string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool discoverServerIp(string& serverIp) {
    SOCKET discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (discoverySocket == INVALID_SOCKET) {
        return false;
    }

    BOOL broadcastEnable = TRUE;
    setsockopt(discoverySocket, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastEnable, sizeof(broadcastEnable));

    DWORD timeoutMs = 2000;
    setsockopt(discoverySocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
    inet_pton(AF_INET, "255.255.255.255", &broadcastAddr.sin_addr);

    sendto(
        discoverySocket,
        DISCOVERY_REQUEST,
        static_cast<int>(strlen(DISCOVERY_REQUEST)),
        0,
        (sockaddr*)&broadcastAddr,
        sizeof(broadcastAddr)
    );

    char buffer[256];
    sockaddr_in responderAddr{};
    int responderLen = sizeof(responderAddr);
    int bytes = recvfrom(
        discoverySocket,
        buffer,
        sizeof(buffer) - 1,
        0,
        (sockaddr*)&responderAddr,
        &responderLen
    );
    closesocket(discoverySocket);

    if (bytes <= 0) {
        return false;
    }

    buffer[bytes] = '\0';
    if (string(buffer) != DISCOVERY_RESPONSE) {
        return false;
    }

    char ipBuffer[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &responderAddr.sin_addr, ipBuffer, sizeof(ipBuffer)) == nullptr) {
        return false;
    }

    serverIp = ipBuffer;
    return true;
}

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

        string incoming(buffer, bytes);
        if (startsWith(incoming, FROM_PREFIX)) {
            string payload = incoming.substr(strlen(FROM_PREFIX));
            size_t delimiterPos = payload.find('|');
            if (delimiterPos != string::npos) {
                string senderName = payload.substr(0, delimiterPos);
                string messageText = payload.substr(delimiterPos + 1);
                cout << "\n[" << senderName << "] " << messageText << endl;
            } else {
                cout << "\n>> " << incoming << endl;
            }
        } else if (startsWith(incoming, SYSTEM_PREFIX)) {
            string notice = incoming.substr(strlen(SYSTEM_PREFIX));
            cout << "\n[system] " << notice << endl;
        } else {
            cout << "\n>> " << incoming << endl;
        }
        cout.flush();
    }
}

int main(int argc, char* argv[]) {
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

    // Ưu tiên IP từ tham số chạy: client.exe <server_ip>
    // Nếu không có, thử tự tìm server trong LAN bằng UDP broadcast.
    string serverIp;
    if (argc >= 2) {
        serverIp = argv[1];
        cout << "Using server IP from argument: " << serverIp << "\n";
    } else if (discoverServerIp(serverIp)) {
        cout << "Discovered server in LAN: " << serverIp << "\n";
    } else {
        serverIp = "127.0.0.1";
        cout << "No LAN server discovered, fallback to localhost (" << serverIp << ")\n";
    }

    // Cấu hình server (cùng PORT với Protocol.h / server)
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(PORT));
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) != 1) {
        cout << "Invalid server IP: " << serverIp << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Kết nối server
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connection failed!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!\n";

    string username;
    if (argc >= 3) {
        username = argv[2];
    } else {
        cout << "Enter your username: ";
        getline(cin, username);
    }

    string authPayload = string(AUTH_PREFIX) + username;
    if (send(sock, authPayload.c_str(), static_cast<int>(authPayload.length()), 0) == SOCKET_ERROR) {
        cout << "Authentication failed!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Thread nhận tin nhắn
    thread t(receiveMessages, sock);
    t.detach();

    string msg;

    // Gửi tin nhắn
    while (true) {
        getline(cin, msg);

        if (!msg.empty()) {
            string outgoing = string(MSG_PREFIX) + msg;
            int result = send(sock, outgoing.c_str(), static_cast<int>(outgoing.length()), 0);

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
