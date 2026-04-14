#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>

#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cctype>
#include <msclr/marshal_cppstd.h>
#include <msclr/lock.h>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Threading;
using namespace System::IO;

static std::string trimNative(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

static std::string sanitizeUsername(const std::string& rawName) {
    std::string input = trimNative(rawName);
    std::string result;
    result.reserve(input.size());

    for (char ch : input) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (isalnum(c) || ch == '_' || ch == '-' || ch == '.')
            result.push_back(ch);
    }

    if (result.empty()) result = "guest";
    if (result.size() > static_cast<size_t>(MAX_USERNAME_LENGTH))
        result.resize(MAX_USERNAME_LENGTH);
    return result;
}

static std::string getRemoteEndpoint(SOCKET clientSocket) {
    sockaddr_in addr{};
    int addrLen = sizeof(addr);
    if (getpeername(clientSocket, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        return "unknown";

    char ipBuf[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &addr.sin_addr, ipBuf, sizeof(ipBuf)) == nullptr)
        return "unknown";

    return std::string(ipBuf) + ":" + std::to_string(ntohs(addr.sin_port));
}

public ref class ServerForm : public Form {
private:
    RichTextBox^ rtbLog;
    ListBox^ lbClients;
    Label^ lblStatus;
    Label^ lblClientCount;
    Button^ btnToggle;
    Button^ btnClearHistory;

    SOCKET serverSocket;
    SOCKET discoverySocket;
    bool isRunning;
    std::unordered_map<SOCKET, int>* clientMap;
    std::unordered_map<SOCKET, std::string>* clientNativeNames;
    std::mutex* clientsMutex;

    System::Collections::Concurrent::ConcurrentDictionary<int, String^>^ clientNames;
    int nextId;
    Thread^ acceptThread;
    Thread^ discoveryThread;

    String^ historyPath = "chat_history.txt";

public:
    ServerForm() {
        serverSocket = INVALID_SOCKET;
        discoverySocket = INVALID_SOCKET;
        isRunning = false;
        nextId = 1;
        clientMap = new std::unordered_map<SOCKET, int>();
        clientNativeNames = new std::unordered_map<SOCKET, std::string>();
        clientsMutex = new std::mutex();
        clientNames = gcnew System::Collections::Concurrent::ConcurrentDictionary<int, String^>();

        InitializeComponent();
        WSADATA wsa;
        (void)WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    ~ServerForm() {
        StopServer();
        delete clientMap;
        delete clientNativeNames;
        delete clientsMutex;
        WSACleanup();
    }

protected:
    !ServerForm() { this->~ServerForm(); }

private:
    void InitializeComponent() {
        this->Text = "TCP Chat Server - Management";
        this->Size = System::Drawing::Size(900, 650);
        this->BackColor = Color::FromArgb(25, 25, 25);
        this->ForeColor = Color::White;
        this->Font = gcnew System::Drawing::Font("Segoe UI", 10);
        this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
        this->StartPosition = FormStartPosition::CenterScreen;

        Label^ lblTitle = gcnew Label();
        lblTitle->Text = "SERVER MANAGEMENT";
        lblTitle->Font = gcnew System::Drawing::Font("Segoe UI Semibold", 18);
        lblTitle->ForeColor = Color::FromArgb(0, 150, 250);
        lblTitle->Location = Point(20, 20);
        lblTitle->AutoSize = true;
        this->Controls->Add(lblTitle);

        lblStatus = gcnew Label();
        lblStatus->Text = L"Trạng thái: ĐANG DỪNG";
        lblStatus->ForeColor = Color::IndianRed;
        lblStatus->Location = Point(20, 65);
        lblStatus->AutoSize = true;
        this->Controls->Add(lblStatus);

        btnToggle = gcnew Button();
        btnToggle->Text = L"BẮT ĐẦU SERVER";
        btnToggle->Location = Point(700, 25);
        btnToggle->Size = System::Drawing::Size(165, 45);
        btnToggle->FlatStyle = FlatStyle::Flat;
        btnToggle->BackColor = Color::FromArgb(0, 120, 215);
        btnToggle->Click += gcnew EventHandler(this, &ServerForm::OnToggleClick);
        this->Controls->Add(btnToggle);

        btnClearHistory = gcnew Button();
        btnClearHistory->Text = L"XÓA LỊCH SỬ FILE";
        btnClearHistory->Location = Point(525, 25);
        btnClearHistory->Size = System::Drawing::Size(165, 45);
        btnClearHistory->FlatStyle = FlatStyle::Flat;
        btnClearHistory->BackColor = Color::FromArgb(60, 60, 60);
        btnClearHistory->Click += gcnew EventHandler(this, &ServerForm::OnClearHistoryClick);
        this->Controls->Add(btnClearHistory);

        rtbLog = gcnew RichTextBox();
        rtbLog->Location = Point(20, 130);
        rtbLog->Size = System::Drawing::Size(620, 460);
        rtbLog->BackColor = Color::FromArgb(15, 15, 15);
        rtbLog->ForeColor = Color::FromArgb(200, 200, 200);
        rtbLog->ReadOnly = true;
        this->Controls->Add(rtbLog);

        lbClients = gcnew ListBox();
        lbClients->Location = Point(655, 130);
        lbClients->Size = System::Drawing::Size(210, 460);
        lbClients->BackColor = Color::FromArgb(30, 30, 30);
        lbClients->ForeColor = Color::LightGreen;
        this->Controls->Add(lbClients);
    }

    void SaveMessageToFile(String^ msg) {
        try {
            File::AppendAllText(historyPath, msg + Environment::NewLine,
                                System::Text::Encoding::UTF8);
        }
        catch (Exception^ ex) { AddLog(L"Lỗi ghi file: " + ex->Message); }
    }

    void OnClearHistoryClick(Object^ sender, EventArgs^ e) {
        try {
            if (File::Exists(historyPath)) {
                File::Delete(historyPath);
                AddLog(L"HỆ THỐNG: Đã xóa file lịch sử trò chuyện.");
                MessageBox::Show(L"Đã xóa file chat_history.txt thành công!",
                    L"Thông báo", MessageBoxButtons::OK, MessageBoxIcon::Information);
            }
            else {
                MessageBox::Show(L"Không tìm thấy file lịch sử để xóa.",
                    L"Thông báo", MessageBoxButtons::OK, MessageBoxIcon::Warning);
            }
        }
        catch (Exception^ ex) {
            MessageBox::Show(L"Không thể xóa file: " + ex->Message,
                L"Lỗi", MessageBoxButtons::OK, MessageBoxIcon::Error);
        }
    }

    void AddLog(String^ msg) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew Action<String^>(this, &ServerForm::AddLog), msg);
            return;
        }
        rtbLog->AppendText("[" + DateTime::Now.ToString("HH:mm:ss") + "] " + msg + "\n");
        rtbLog->ScrollToCaret();
    }

    void UpdateClientList() {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew Action(this, &ServerForm::UpdateClientList));
            return;
        }
        lbClients->Items->Clear();
        for each (auto pair in clientNames) lbClients->Items->Add(pair.Value);
    }

    bool UsernameExistsLocked(const std::string& name) {
        for (auto const& [sock, n] : *clientNativeNames) {
            if (n == name) return true;
        }
        return false;
    }

    std::string BuildUniqueUsernameLocked(const std::string& requestedName) {
        std::string baseName = sanitizeUsername(requestedName);
        if (!UsernameExistsLocked(baseName)) return baseName;

        for (int suffix = 2;; ++suffix) {
            std::string suffixText = "#" + std::to_string(suffix);
            std::string candidateBase = baseName;
            if (candidateBase.size() + suffixText.size() > static_cast<size_t>(MAX_USERNAME_LENGTH)) {
                size_t allowed = static_cast<size_t>(MAX_USERNAME_LENGTH) - suffixText.size();
                candidateBase = candidateBase.substr(0, allowed);
            }
            std::string candidate = candidateBase + suffixText;
            if (!UsernameExistsLocked(candidate)) return candidate;
        }
    }

    void BroadcastOnlineSummary() {
        String^ summary;
        {
            std::lock_guard<std::mutex> lock(*clientsMutex);
            if (clientMap->empty()) {
                summary = "Online (0)";
            }
            else {
                summary = "Online (" + clientMap->size() + "): ";
                bool first = true;
                for each (auto pair in clientNames) {
                    if (!first) summary += ", ";
                    first = false;
                    summary += pair.Value;
                }
            }
        }
        Broadcast(msclr::interop::marshal_as<String^>(SYSTEM_PREFIX) + summary, INVALID_SOCKET);
        AddLog(summary);
    }

    void StartServer() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(PORT));
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            AddLog(L"Bind thất bại! Có thể port đang bị chiếm.");
            return;
        }
        listen(serverSocket, SOMAXCONN);

        discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in daddr{};
        daddr.sin_family = AF_INET;
        daddr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
        daddr.sin_addr.s_addr = INADDR_ANY;
        bind(discoverySocket, (sockaddr*)&daddr, sizeof(daddr));

        isRunning = true;
        acceptThread = gcnew Thread(gcnew ThreadStart(this, &ServerForm::AcceptLoop));
        acceptThread->Start();
        discoveryThread = gcnew Thread(gcnew ThreadStart(this, &ServerForm::DiscoveryLoop));
        discoveryThread->Start();

        lblStatus->Text = L"Trạng thái: ĐANG CHẠY (Port " + PORT + ")";
        lblStatus->ForeColor = Color::LightGreen;
        btnToggle->Text = L"DỪNG SERVER";
        btnToggle->BackColor = Color::IndianRed;
        AddLog(L"Server đã khởi động trên TCP port " + PORT +
               ", discovery UDP port " + DISCOVERY_PORT + ".");
    }

    void StopServer() {
        isRunning = false;
        if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
        if (discoverySocket != INVALID_SOCKET) closesocket(discoverySocket);
        {
            std::lock_guard<std::mutex> lock(*clientsMutex);
            for (auto const& [sock, id] : *clientMap) closesocket(sock);
            clientMap->clear();
            clientNativeNames->clear();
            clientNames->Clear();
        }
        lblStatus->Text = L"Trạng thái: ĐANG DỪNG";
        lblStatus->ForeColor = Color::IndianRed;
        btnToggle->Text = L"BẮT ĐẦU SERVER";
        btnToggle->BackColor = Color::FromArgb(0, 120, 215);
        UpdateClientList();
        AddLog(L"Server đã dừng.");
    }

    void AcceptLoop() {
        while (isRunning) {
            SOCKET cs = accept(serverSocket, NULL, NULL);
            if (cs == INVALID_SOCKET) break;
            AddLog("Client kết nối từ " +
                   msclr::interop::marshal_as<String^>(getRemoteEndpoint(cs)) +
                   ", chờ xác thực...");
            Thread^ t = gcnew Thread(gcnew ParameterizedThreadStart(this, &ServerForm::HandleClient));
            t->Start(cs);
        }
    }

    void DiscoveryLoop() {
        char buf[256];
        while (isRunning) {
            sockaddr_in caddr{};
            int clen = sizeof(caddr);
            int b = recvfrom(discoverySocket, buf, 255, 0, (sockaddr*)&caddr, &clen);
            if (b > 0 && std::string(buf, b) == DISCOVERY_REQUEST) {
                sendto(discoverySocket, DISCOVERY_RESPONSE,
                       static_cast<int>(strlen(DISCOVERY_RESPONSE)),
                       0, (sockaddr*)&caddr, clen);
            }
        }
    }

    void HandleClient(Object^ socketObj) {
        SOCKET s = (SOCKET)(unsigned __int64)socketObj;
        char buf[1024];
        int myId = 0;
        String^ myName = "";
        std::string nativeName;

        String^ authPrefix = msclr::interop::marshal_as<String^>(AUTH_PREFIX);
        String^ msgPrefix  = msclr::interop::marshal_as<String^>(MSG_PREFIX);

        try {
            while (isRunning) {
                int b = recv(s, buf, 1023, 0);
                if (b <= 0) break;

                cli::array<unsigned char>^ bytes = gcnew cli::array<unsigned char>(b);
                System::Runtime::InteropServices::Marshal::Copy(IntPtr(buf), bytes, 0, b);
                String^ rawStr = System::Text::Encoding::UTF8->GetString(bytes);

                if (rawStr->StartsWith(authPrefix)) {
                    std::string requestedName = msclr::interop::marshal_as<std::string>(
                        rawStr->Substring(authPrefix->Length));

                    {
                        std::lock_guard<std::mutex> lock(*clientsMutex);
                        myId = nextId++;
                        nativeName = BuildUniqueUsernameLocked(requestedName);
                        myName = msclr::interop::marshal_as<String^>(nativeName)
                                 + " (#" + myId + ")";
                        (*clientMap)[s] = myId;
                        (*clientNativeNames)[s] = nativeName;
                        clientNames->TryAdd(myId, myName);
                    }

                    AddLog("LOGIN: " + myName + " từ " +
                           msclr::interop::marshal_as<String^>(getRemoteEndpoint(s)));
                    UpdateClientList();

                    String^ welcomeMsg = msclr::interop::marshal_as<String^>(SYSTEM_PREFIX)
                        + L"Chào mừng " + msclr::interop::marshal_as<String^>(nativeName)
                        + L" đã tham gia!";
                    Broadcast(welcomeMsg, INVALID_SOCKET);
                    BroadcastOnlineSummary();
                }
                else if (rawStr->Trim() == "/history") {
                    if (File::Exists(historyPath)) {
                        cli::array<String^>^ lines = File::ReadAllLines(
                            historyPath, System::Text::Encoding::UTF8);
                        for each (String^ line in lines) {
                            String^ histLine = msclr::interop::marshal_as<String^>(SYSTEM_PREFIX)
                                + "[HISTORY] " + line + "\n";
                            cli::array<unsigned char>^ bMsg =
                                System::Text::Encoding::UTF8->GetBytes(histLine);
                            pin_ptr<unsigned char> p = &bMsg[0];
                            send(s, (const char*)p, bMsg->Length, 0);
                        }
                    }
                    else {
                        std::string noHis = std::string(SYSTEM_PREFIX)
                            + "Chưa có lịch sử trò chuyện.\n";
                        send(s, noHis.c_str(), (int)noHis.size(), 0);
                    }
                }
                else if (rawStr->StartsWith(msgPrefix)) {
                    String^ content = rawStr->Substring(msgPrefix->Length);
                    String^ fullMsg = myName + ": " + content;
                    SaveMessageToFile(fullMsg);
                    AddLog("MSG from " + myName + ": " + content);

                    String^ outgoing = msclr::interop::marshal_as<String^>(FROM_PREFIX)
                        + myName + "|" + content;
                    Broadcast(outgoing, s);
                }
            }
        }
        catch (Exception^ ex) { AddLog(L"Lỗi client: " + ex->Message); }

        if (myId > 0) {
            {
                std::lock_guard<std::mutex> lock(*clientsMutex);
                clientMap->erase(s);
                clientNativeNames->erase(s);
                String^ dummy;
                clientNames->TryRemove(myId, dummy);
            }
            AddLog("LOGOUT: " + myName);
            UpdateClientList();

            String^ leaveMsg = msclr::interop::marshal_as<String^>(SYSTEM_PREFIX)
                + myName + L" đã thoát.";
            Broadcast(leaveMsg, INVALID_SOCKET);
            BroadcastOnlineSummary();
        }
        closesocket(s);
    }

    void Broadcast(String^ msg, SOCKET excluded) {
        cli::array<unsigned char>^ bytes = System::Text::Encoding::UTF8->GetBytes(msg + "\n");
        pin_ptr<unsigned char> p = &bytes[0];
        std::lock_guard<std::mutex> lock(*clientsMutex);
        for (auto const& [sock, id] : *clientMap) {
            if (sock != excluded) send(sock, (const char*)p, bytes->Length, 0);
        }
    }

    void OnToggleClick(Object^ sender, EventArgs^ e) {
        if (!isRunning) StartServer(); else StopServer();
    }
};

[STAThreadAttribute]
int main(cli::array<System::String^>^ args) {
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew ServerForm());
    return 0;
}
