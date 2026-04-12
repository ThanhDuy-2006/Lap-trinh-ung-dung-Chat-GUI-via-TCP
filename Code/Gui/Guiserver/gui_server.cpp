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
#include <msclr/marshal_cppstd.h>
#include <msclr/lock.h>

#pragma comment(lib, "ws2_32.lib")

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Threading;
using namespace System::IO;

public ref class ServerForm : public Form {
private:
    RichTextBox^ rtbLog;
    ListBox^ lbClients;
    Label^ lblStatus;
    Label^ lblClientCount;
    Button^ btnToggle;
    Button^ btnClearHistory; // Nút xóa lịch sử mới

    SOCKET serverSocket;
    SOCKET discoverySocket;
    bool isRunning;
    std::unordered_map<SOCKET, int>* clientMap;
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
        clientsMutex = new std::mutex();
        clientNames = gcnew System::Collections::Concurrent::ConcurrentDictionary<int, String^>();

        InitializeComponent();
        WSADATA wsa;
        (void)WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    ~ServerForm() {
        StopServer();
        delete clientMap;
        delete clientsMutex;
        WSACleanup();
    }

protected:
    !ServerForm() { this->~ServerForm(); }

private:
    void InitializeComponent() {
        this->Text = "TCP Chat Server Pro - UTF8 & Management";
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
        lblStatus->Text = "Trạng thái: ĐANG DỪNG";
        lblStatus->ForeColor = Color::IndianRed;
        lblStatus->Location = Point(20, 65);
        lblStatus->AutoSize = true;
        this->Controls->Add(lblStatus);

        btnToggle = gcnew Button();
        btnToggle->Text = "BẮT ĐẦU SERVER";
        btnToggle->Location = Point(700, 25);
        btnToggle->Size = System::Drawing::Size(165, 45);
        btnToggle->FlatStyle = FlatStyle::Flat;
        btnToggle->BackColor = Color::FromArgb(0, 120, 215);
        btnToggle->Click += gcnew EventHandler(this, &ServerForm::OnToggleClick);
        this->Controls->Add(btnToggle);


        btnClearHistory = gcnew Button();
        btnClearHistory->Text = "XÓA LỊCH SỬ FILE";
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
            File::AppendAllText(historyPath, msg + Environment::NewLine, System::Text::Encoding::UTF8);
        }
        catch (Exception^ ex) { AddLog("Lỗi ghi file: " + ex->Message); }
    }

    // HÀM XỬ LÝ XÓA LỊCH SỬ
    void OnClearHistoryClick(Object^ sender, EventArgs^ e) {
        try {
            if (File::Exists(historyPath)) {
                File::Delete(historyPath);
                AddLog("HỆ THỐNG: Đã xóa file lịch sử trò chuyện.");
                MessageBox::Show("Đã xóa file chat_history.txt thành công!", "Thông báo", MessageBoxButtons::OK, MessageBoxIcon::Information);
            }
            else {
                MessageBox::Show("Không tìm thấy file lịch sử để xóa.", "Thông báo", MessageBoxButtons::OK, MessageBoxIcon::Warning);
            }
        }
        catch (Exception^ ex) {
            MessageBox::Show("Không thể xóa file: " + ex->Message, "Lỗi", MessageBoxButtons::OK, MessageBoxIcon::Error);
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

    void StartServer() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9050);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return;
        listen(serverSocket, SOMAXCONN);

        discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in daddr{};
        daddr.sin_family = AF_INET;
        daddr.sin_port = htons(9051);
        daddr.sin_addr.s_addr = INADDR_ANY;
        bind(discoverySocket, (sockaddr*)&daddr, sizeof(daddr));

        isRunning = true;
        acceptThread = gcnew Thread(gcnew ThreadStart(this, &ServerForm::AcceptLoop));
        acceptThread->Start();
        discoveryThread = gcnew Thread(gcnew ThreadStart(this, &ServerForm::DiscoveryLoop));
        discoveryThread->Start();

        lblStatus->Text = "Trạng thái: ĐANG CHẠY (Port 9050)";
        lblStatus->ForeColor = Color::LightGreen;
        btnToggle->Text = "DỪNG SERVER";
        btnToggle->BackColor = Color::IndianRed;
        AddLog("Server đã khởi động.");
    }

    void StopServer() {
        isRunning = false;
        if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
        if (discoverySocket != INVALID_SOCKET) closesocket(discoverySocket);
        {
            std::lock_guard<std::mutex> lock(*clientsMutex);
            for (auto const& [sock, id] : *clientMap) closesocket(sock);
            clientMap->clear(); clientNames->Clear();
        }
        lblStatus->Text = "Trạng thái: ĐANG DỪNG";
        lblStatus->ForeColor = Color::IndianRed;
        btnToggle->Text = "BẮT ĐẦU SERVER";
        btnToggle->BackColor = Color::FromArgb(0, 120, 215);
        UpdateClientList();
        AddLog("Server đã dừng.");
    }

    void AcceptLoop() {
        while (isRunning) {
            SOCKET cs = accept(serverSocket, NULL, NULL);
            if (cs == INVALID_SOCKET) break;
            Thread^ t = gcnew Thread(gcnew ParameterizedThreadStart(this, &ServerForm::HandleClient));
            t->Start(cs);
        }
    }

    void DiscoveryLoop() {
        char buf[256];
        while (isRunning) {
            sockaddr_in caddr{}; int clen = sizeof(caddr);
            int b = recvfrom(discoverySocket, buf, 255, 0, (sockaddr*)&caddr, &clen);
            if (b > 0 && std::string(buf, b) == "CHAT_DISCOVER_REQUEST") {
                sendto(discoverySocket, "CHAT_DISCOVER_RESPONSE", 22, 0, (sockaddr*)&caddr, clen);
            }
        }
    }

    void HandleClient(Object^ socketObj) {
        SOCKET s = (SOCKET)(unsigned __int64)socketObj;
        char buf[1024];
        int myId = 0; String^ myName = "";

        try {
            while (isRunning) {
                int b = recv(s, buf, 1023, 0);
                if (b <= 0) break;

                cli::array<unsigned char>^ bytes = gcnew cli::array<unsigned char>(b);
                System::Runtime::InteropServices::Marshal::Copy(IntPtr(buf), bytes, 0, b);
                String^ rawStr = System::Text::Encoding::UTF8->GetString(bytes);

                if (rawStr->StartsWith("AUTH|")) {
                    String^ name = rawStr->Substring(5);
                    {
                        std::lock_guard<std::mutex> lock(*clientsMutex);
                        myId = nextId++;
                        myName = name + " (#" + myId + ")";
                        (*clientMap)[s] = myId;
                        clientNames->TryAdd(myId, myName);
                    }
                    AddLog("LOGIN: " + myName); UpdateClientList();
                    Broadcast("SYSTEM|Chào mừng " + name + " đã tham gia!", INVALID_SOCKET);
                }
                else if (rawStr->Trim() == "/history") {
                    if (File::Exists(historyPath)) {
                        cli::array<String^>^ lines = File::ReadAllLines(historyPath, System::Text::Encoding::UTF8);
                        for each (String ^ line in lines) {
                            cli::array<unsigned char>^ bMsg = System::Text::Encoding::UTF8->GetBytes("SYSTEM|[HISTORY] " + line + "\n");
                            pin_ptr<unsigned char> p = &bMsg[0];
                            send(s, (const char*)p, bMsg->Length, 0);
                        }
                    }
                    else {
                        std::string noHis = "SYSTEM|Chưa có lịch sử trò chuyện.\n";
                        send(s, noHis.c_str(), (int)noHis.size(), 0);
                    }
                }
                else if (rawStr->StartsWith("MSG|")) {
                    String^ content = rawStr->Substring(4);
                    String^ fullMsg = myName + ": " + content;
                    SaveMessageToFile(fullMsg);
                    AddLog("MSG from " + myName + ": " + content);
                    Broadcast("FROM|" + myName + "|" + content, s);
                }
            }
        }
        catch (Exception^ ex) { AddLog("Lỗi client: " + ex->Message); }

        if (myId > 0) {
            {
                std::lock_guard<std::mutex> lock(*clientsMutex);
                clientMap->erase(s); String^ dummy; clientNames->TryRemove(myId, dummy);
            }
            AddLog("LOGOUT: " + myName); UpdateClientList();
            Broadcast("SYSTEM|" + myName + " đã thoát.", INVALID_SOCKET);
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