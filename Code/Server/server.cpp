#include <iostream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <windows.h>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct ClientInfo {
    int id = 0;
    string name;
    SOCKET socket = INVALID_SOCKET;
};

unordered_map<SOCKET, ClientInfo> clients;
mutex clientsMutex;
atomic<int> nextClientId{1};
mutex logMutex;
string activityLogPath;
void broadcast(const string& msg, SOCKET excludedSocket = INVALID_SOCKET);

bool startsWith(const string& text, const string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    tm localTime{};
    localtime_s(&localTime, &currentTime);

    stringstream ss;
    ss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

string getExecutableDirectory() {
    char modulePath[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return ".";
    }
    string fullPath(modulePath);
    size_t separator = fullPath.find_last_of("\\/");
    if (separator == string::npos) {
        return ".";
    }
    return fullPath.substr(0, separator);
}

void logRealtime(const string& eventText) {
    const string line = "[" + getCurrentTimestamp() + "] " + eventText;
    lock_guard<mutex> lock(logMutex);

    cout << line << endl;
    if (!activityLogPath.empty()) {
        ofstream out(activityLogPath, ios::app);
        if (out.is_open()) {
            out << line << "\n";
        }
    }
}

string trim(const string& value) {
    size_t start = 0;
    while (start < value.size() && isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

string sanitizeUsername(const string& rawName) {
    string input = trim(rawName);
    string result;
    result.reserve(input.size());

    for (char ch : input) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (isalnum(c) || ch == '_' || ch == '-' || ch == '.') {
            result.push_back(ch);
        }
    }

    if (result.empty()) {
        result = "guest";
    }

    if (result.size() > static_cast<size_t>(MAX_USERNAME_LENGTH)) {
        result.resize(MAX_USERNAME_LENGTH);
    }

    return result;
}

bool usernameExistsLocked(const string& name) {
    for (const auto& entry : clients) {
        if (entry.second.name == name) {
            return true;
        }
    }
    return false;
}

string buildUniqueUsernameLocked(const string& requestedName) {
    string baseName = sanitizeUsername(requestedName);
    if (!usernameExistsLocked(baseName)) {
        return baseName;
    }

    for (int suffix = 2;; ++suffix) {
        string suffixText = "#" + to_string(suffix);
        string candidateBase = baseName;
        if (suffixText.size() >= static_cast<size_t>(MAX_USERNAME_LENGTH)) {
            candidateBase.clear();
            suffixText = suffixText.substr(0, static_cast<size_t>(MAX_USERNAME_LENGTH));
        } else if (candidateBase.size() + suffixText.size() > static_cast<size_t>(MAX_USERNAME_LENGTH)) {
            size_t allowedBaseLength = static_cast<size_t>(MAX_USERNAME_LENGTH) - suffixText.size();
            candidateBase = candidateBase.substr(0, allowedBaseLength);
        }

        string candidate = candidateBase + suffixText;
        if (!usernameExistsLocked(candidate)) {
            return candidate;
        }
    }
}

void sendToClient(SOCKET clientSocket, const string& payload) {
    send(clientSocket, payload.c_str(), static_cast<int>(payload.size()), 0);
}

string buildOnlineSummaryLocked() {
    if (clients.empty()) {
        return "Online (0): none";
    }

    string summary = "Online (" + to_string(clients.size()) + "): ";
    bool first = true;
    for (const auto& entry : clients) {
        if (!first) {
            summary += ", ";
        }
        first = false;
        summary += entry.second.name + "(#" + to_string(entry.second.id) + ")";
    }
    return summary;
}

string getOnlineSummary() {
    lock_guard<mutex> lock(clientsMutex);
    return buildOnlineSummaryLocked();
}

void broadcastOnlineSummary() {
    const string summary = getOnlineSummary();
    const string payload = string(SYSTEM_PREFIX) + summary;
    broadcast(payload, INVALID_SOCKET);
    logRealtime(summary);
}

string getRemoteEndpoint(SOCKET clientSocket) {
    sockaddr_in addr{};
    int addrLen = sizeof(addr);
    if (getpeername(clientSocket, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR) {
        return "unknown";
    }

    char ipBuffer[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &addr.sin_addr, ipBuffer, sizeof(ipBuffer)) == nullptr) {
        return "unknown";
    }

    return string(ipBuffer) + ":" + to_string(ntohs(addr.sin_port));
}

void runDiscoveryResponder() {
    SOCKET discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (discoverySocket == INVALID_SOCKET) {
        cout << "Discovery socket creation failed!\n";
        return;
    }

    sockaddr_in discoveryAddr{};
    discoveryAddr.sin_family = AF_INET;
    discoveryAddr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
    discoveryAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(discoverySocket, (sockaddr*)&discoveryAddr, sizeof(discoveryAddr)) == SOCKET_ERROR) {
        cout << "Discovery bind failed!\n";
        closesocket(discoverySocket);
        return;
    }

    char buffer[256];
    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        int bytes = recvfrom(discoverySocket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&clientAddr, &clientLen);
        if (bytes <= 0) {
            continue;
        }

        buffer[bytes] = '\0';
        if (string(buffer) == DISCOVERY_REQUEST) {
            sendto(
                discoverySocket,
                DISCOVERY_RESPONSE,
                static_cast<int>(strlen(DISCOVERY_RESPONSE)),
                0,
                (sockaddr*)&clientAddr,
                clientLen
            );
        }
    }
}

bool removeClient(SOCKET s, ClientInfo& removedClient) {
    lock_guard<mutex> lock(clientsMutex);
    auto it = clients.find(s);
    if (it == clients.end()) {
        return false;
    }
    removedClient = it->second;
    clients.erase(it);
    return true;
}

void broadcast(const string& msg, SOCKET excludedSocket) {
    lock_guard<mutex> lock(clientsMutex);
    for (const auto& entry : clients) {
        SOCKET clientSocket = entry.first;
        if (clientSocket != excludedSocket) {
            send(clientSocket, msg.c_str(), static_cast<int>(msg.size()), 0);
        }
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[1024];
    bool authenticated = false;
    ClientInfo currentClient;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytes <= 0) {
            if (authenticated) {
                ClientInfo removedClient;
                if (removeClient(clientSocket, removedClient)) {
                    logRealtime("LOGOUT client=" + removedClient.name + " (#" + to_string(removedClient.id) + ")");
                    broadcast(string(SYSTEM_PREFIX) + removedClient.name + " left the chat.", clientSocket);
                    broadcastOnlineSummary();
                }
            } else {
                logRealtime("Client disconnected before authentication");
            }
            break;
        }

        string msg(buffer, bytes);

        if (!authenticated) {
            if (!startsWith(msg, AUTH_PREFIX)) {
                sendToClient(clientSocket, string(SYSTEM_PREFIX) + "Please authenticate first.");
                continue;
            }

            string requestedName = msg.substr(strlen(AUTH_PREFIX));

            {
                lock_guard<mutex> lock(clientsMutex);
                currentClient.id = nextClientId++;
                currentClient.name = buildUniqueUsernameLocked(requestedName);
                currentClient.socket = clientSocket;
                clients[clientSocket] = currentClient;
            }

            authenticated = true;
            sendToClient(
                clientSocket,
                string(SYSTEM_PREFIX) + "Connected as " + currentClient.name + " (#" + to_string(currentClient.id) + ")."
            );
            broadcast(string(SYSTEM_PREFIX) + currentClient.name + " joined the chat.", clientSocket);
            logRealtime(
                "LOGIN client=" + currentClient.name + " (#" + to_string(currentClient.id) + "), from " + getRemoteEndpoint(clientSocket)
            );
            broadcastOnlineSummary();
            continue;
        }

        if (!startsWith(msg, MSG_PREFIX)) {
            sendToClient(clientSocket, string(SYSTEM_PREFIX) + "Invalid message format.");
            continue;
        }

        string body = trim(msg.substr(strlen(MSG_PREFIX)));
        if (body.empty()) {
            continue;
        }

        string outgoing = string(FROM_PREFIX) + currentClient.name + "|" + body;
        logRealtime("MESSAGE " + currentClient.name + " (#" + to_string(currentClient.id) + "): " + body);
        broadcast(outgoing, clientSocket);
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "WSAStartup failed!\n";
        return 1;
    }

    const string exeDir = getExecutableDirectory();
    activityLogPath = exeDir + "\\client_activity_log.txt";
    {
        ofstream createIfMissing(activityLogPath, ios::app);
    }
    logRealtime("SERVER_START listening on TCP " + to_string(PORT) + ", discovery UDP " + to_string(DISCOVERY_PORT));

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

    logRealtime("Server started on port " + to_string(PORT));
    logRealtime("LAN discovery active on UDP port " + to_string(DISCOVERY_PORT));

    thread discoveryThread(runDiscoveryResponder);
    discoveryThread.detach();

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);

        if (clientSocket == INVALID_SOCKET) {
            logRealtime("Accept failed");
            continue;
        }

        logRealtime("Client connected, waiting for authentication...");

        thread t(handleClient, clientSocket);
        t.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
}
