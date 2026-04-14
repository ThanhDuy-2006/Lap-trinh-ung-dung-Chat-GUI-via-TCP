#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <msclr/marshal_cppstd.h>
#include "../Shared/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Threading;
using namespace System::Collections::Generic;

public ref class ChatForm : public Form {
private:
    Panel^ loginPanel;
    Panel^ chatPanel;

    TextBox^ txtUsername;
    TextBox^ txtServerIp;
    Button^ btnConnect;
    Label^ lblStatus;
    RichTextBox^ rtbLog;

    RichTextBox^ rtbMessages;
    TextBox^ txtInput;
    Button^ btnSend;
    Button^ btnDisconnect;
    Label^ lblOnlineStatus;

    SOCKET g_sock = INVALID_SOCKET;
    Thread^ receiveThread;
    bool isConnected = false;

public:
    ChatForm() {
        InitializeComponent();
        WSADATA wsa;
        (void)WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    ~ChatForm() {
        if (g_sock != INVALID_SOCKET) closesocket(g_sock);
        WSACleanup();
    }

private:
    void OnHistoryClick(Object^ sender, EventArgs^ e) {
        if (!isConnected) return;

        String^ req = "/history";
        cli::array<unsigned char>^ bytes = System::Text::Encoding::UTF8->GetBytes(req);

        pin_ptr<unsigned char> p = &bytes[0];
        send(g_sock, (const char*)p, bytes->Length, 0);
    }

    void InitializeComponent() {
        this->Text = "TCP Chat Application";
        this->Size = System::Drawing::Size(800, 600);
        this->BackColor = Color::FromArgb(30, 30, 30);
        this->ForeColor = Color::White;
        this->Font = gcnew System::Drawing::Font("Segoe UI", 10);
        this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
        this->MaximizeBox = false;

        // --- Login Panel ---
        loginPanel = gcnew Panel();
        loginPanel->Dock = DockStyle::Fill;

        Label^ lblTitle = gcnew Label();
        lblTitle->Text = "TCP CHAT CLIENT";
        lblTitle->Font = gcnew System::Drawing::Font("Segoe UI Semibold", 20);
        lblTitle->ForeColor = Color::FromArgb(0, 150, 250);
        lblTitle->Location = Point(20, 20);
        lblTitle->AutoSize = true;
        loginPanel->Controls->Add(lblTitle);

        Label^ lblUser = gcnew Label();
        lblUser->Text = L"Tên hiển thị:";
        lblUser->Location = Point(20, 80);
        loginPanel->Controls->Add(lblUser);

        txtUsername = gcnew TextBox();
        txtUsername->Location = Point(20, 110);
        txtUsername->Size = System::Drawing::Size(250, 30);
        txtUsername->BackColor = Color::FromArgb(45, 45, 45);
        txtUsername->ForeColor = Color::White;
        txtUsername->BorderStyle = BorderStyle::FixedSingle;
        txtUsername->Text = "Guest";
        loginPanel->Controls->Add(txtUsername);

        txtServerIp = gcnew TextBox();
        txtServerIp->Location = Point(20, 180);
        txtServerIp->Size = System::Drawing::Size(250, 30);
        txtServerIp->BackColor = Color::FromArgb(45, 45, 45);
        txtServerIp->ForeColor = Color::White;
        txtServerIp->BorderStyle = BorderStyle::FixedSingle;
        txtServerIp->Visible = false;
        loginPanel->Controls->Add(txtServerIp);

        btnConnect = gcnew Button();
        btnConnect->Text = L"KẾT NỐI SERVER";
        btnConnect->Location = Point(20, 160);
        btnConnect->Size = System::Drawing::Size(250, 45);
        btnConnect->FlatStyle = FlatStyle::Flat;
        btnConnect->BackColor = Color::FromArgb(0, 120, 215);
        btnConnect->FlatAppearance->BorderSize = 0;
        btnConnect->Click += gcnew EventHandler(this, &ChatForm::OnConnectClick);
        loginPanel->Controls->Add(btnConnect);

        rtbLog = gcnew RichTextBox();
        rtbLog->Location = Point(300, 80);
        rtbLog->Size = System::Drawing::Size(460, 460);
        rtbLog->BackColor = Color::FromArgb(20, 20, 20);
        rtbLog->ForeColor = Color::LightGray;
        rtbLog->ReadOnly = true;
        rtbLog->BorderStyle = BorderStyle::None;
        loginPanel->Controls->Add(rtbLog);

        this->Controls->Add(loginPanel);

        // --- Chat Panel ---
        chatPanel = gcnew Panel();
        chatPanel->Dock = DockStyle::Fill;
        chatPanel->Visible = false;

        lblOnlineStatus = gcnew Label();
        lblOnlineStatus->Text = L"Đang trực tuyến: ...";
        lblOnlineStatus->Location = Point(20, 15);
        lblOnlineStatus->AutoSize = true;
        chatPanel->Controls->Add(lblOnlineStatus);

        btnDisconnect = gcnew Button();

        Button^ btnHistory = gcnew Button();
        btnHistory->Text = L"Lịch sử";
        btnHistory->Location = Point(550, 10);
        btnHistory->Size = System::Drawing::Size(100, 30);
        btnHistory->FlatStyle = FlatStyle::Flat;
        btnHistory->BackColor = Color::FromArgb(100, 100, 100);
        btnHistory->Click += gcnew EventHandler(this, &ChatForm::OnHistoryClick);
        chatPanel->Controls->Add(btnHistory);

        btnDisconnect->Text = L"Đăng xuất";
        btnDisconnect->Location = Point(670, 10);
        btnDisconnect->Size = System::Drawing::Size(100, 30);
        btnDisconnect->FlatStyle = FlatStyle::Flat;
        btnDisconnect->BackColor = Color::FromArgb(200, 50, 50);
        btnDisconnect->Click += gcnew EventHandler(this, &ChatForm::OnDisconnectClick);
        chatPanel->Controls->Add(btnDisconnect);

        rtbMessages = gcnew RichTextBox();
        rtbMessages->Location = Point(20, 50);
        rtbMessages->Size = System::Drawing::Size(750, 440);
        rtbMessages->BackColor = Color::FromArgb(35, 35, 35);
        rtbMessages->ForeColor = Color::White;
        rtbMessages->ReadOnly = true;
        rtbMessages->BorderStyle = BorderStyle::None;
        chatPanel->Controls->Add(rtbMessages);

        txtInput = gcnew TextBox();
        txtInput->Location = Point(20, 510);
        txtInput->Size = System::Drawing::Size(650, 40);
        txtInput->BackColor = Color::FromArgb(45, 45, 45);
        txtInput->ForeColor = Color::White;
        txtInput->BorderStyle = BorderStyle::FixedSingle;
        txtInput->KeyDown += gcnew KeyEventHandler(this, &ChatForm::OnInputKeyDown);
        chatPanel->Controls->Add(txtInput);

        btnSend = gcnew Button();
        btnSend->Text = L"GỬI";
        btnSend->Location = Point(680, 510);
        btnSend->Size = System::Drawing::Size(90, 30);
        btnSend->FlatStyle = FlatStyle::Flat;
        btnSend->BackColor = Color::FromArgb(0, 120, 215);
        btnSend->Click += gcnew EventHandler(this, &ChatForm::OnSendClick);
        chatPanel->Controls->Add(btnSend);

        this->Controls->Add(chatPanel);
    }

    void AddLog(String^ msg) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew Action<String^>(this, &ChatForm::AddLog), msg);
            return;
        }
        rtbLog->AppendText("[" + DateTime::Now.ToString("HH:mm:ss") + "] " + msg + "\n");
        rtbLog->SelectionStart = rtbLog->TextLength;
        rtbLog->ScrollToCaret();
    }

    void AddMessage(String^ msg) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew Action<String^>(this, &ChatForm::AddMessage), msg);
            return;
        }
        rtbMessages->AppendText(msg + "\n");
        rtbMessages->SelectionStart = rtbMessages->TextLength;
        rtbMessages->ScrollToCaret();
    }

    bool DiscoverServer(std::string& ip) {
        SOCKET ds = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (ds == INVALID_SOCKET) return false;

        BOOL opt = TRUE;
        setsockopt(ds, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));
        DWORD tv = 2000;
        setsockopt(ds, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
        inet_pton(AF_INET, "255.255.255.255", &addr.sin_addr);

        sendto(ds, DISCOVERY_REQUEST, static_cast<int>(strlen(DISCOVERY_REQUEST)),
               0, (sockaddr*)&addr, sizeof(addr));

        char buf[256];
        sockaddr_in resp{};
        int rlen = sizeof(resp);
        int b = recvfrom(ds, buf, sizeof(buf) - 1, 0, (sockaddr*)&resp, &rlen);
        closesocket(ds);

        if (b <= 0) return false;

        buf[b] = '\0';
        if (std::string(buf) != DISCOVERY_RESPONSE) return false;

        char ipBuf[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &resp.sin_addr, ipBuf, sizeof(ipBuf)) == nullptr)
            return false;

        ip = ipBuf;
        return true;
    }

    void ReceiveLoop() {
        char buf[1024];
        while (isConnected) {
            int b = recv(g_sock, buf, sizeof(buf) - 1, 0);
            if (b <= 0) {
                isConnected = false;
                this->BeginInvoke(gcnew Action(this, &ChatForm::SwitchToLogin));
                AddLog(L"Mất kết nối với Server.");
                break;
            }

            cli::array<unsigned char>^ bytes = gcnew cli::array<unsigned char>(b);
            System::Runtime::InteropServices::Marshal::Copy(IntPtr(buf), bytes, 0, b);
            String^ rawStr = System::Text::Encoding::UTF8->GetString(bytes);

            String^ managedMsg = "";
            String^ systemPrefix = msclr::interop::marshal_as<String^>(SYSTEM_PREFIX);
            String^ fromPrefix = msclr::interop::marshal_as<String^>(FROM_PREFIX);

            if (rawStr->StartsWith(fromPrefix)) {
                String^ p = rawStr->Substring(fromPrefix->Length);
                int d = p->IndexOf("|");
                if (d != -1) {
                    managedMsg = p->Substring(0, d) + ": " + p->Substring(d + 1);
                }
            }
            else if (rawStr->StartsWith(systemPrefix)) {
                String^ content = rawStr->Substring(systemPrefix->Length);

                if (content->StartsWith("[HISTORY]")) {
                    String^ hist = content->Substring(9)->Trim();
                    if (hist->StartsWith("FROM|")) {
                        String^ p = hist->Substring(5);
                        int d = p->IndexOf("|");
                        if (d != -1) {
                            managedMsg = L"[Lịch sử] " + p->Substring(0, d) + ": " + p->Substring(d + 1);
                        }
                    }
                    else {
                        managedMsg = L"[Lịch sử] " + hist;
                    }
                }
                else {
                    managedMsg = L"[Hệ thống] " + content;
                }
            }
            else {
                managedMsg = rawStr;
            }

            if (!String::IsNullOrEmpty(managedMsg)) AddMessage(managedMsg);
        }
    }

    void OnConnectClick(Object^ sender, EventArgs^ e) {
        btnConnect->Enabled = false;
        btnConnect->Text = L"ĐANG KẾT NỐI...";

        Thread^ t = gcnew Thread(gcnew ThreadStart(this, &ChatForm::ConnectAsync));
        t->Start();
    }

    void ConnectAsync() {
        std::string targetIp;
        String^ inputIp = txtServerIp->Text;

        if (String::IsNullOrWhiteSpace(inputIp)) {
            AddLog(L"Đang tìm Server trong LAN...");
            if (!DiscoverServer(targetIp)) {
                targetIp = "127.0.0.1";
                AddLog(L"Không tìm thấy LAN server, dùng localhost.");
            }
            else {
                AddLog(L"Đã tìm thấy Server: " + msclr::interop::marshal_as<String^>(targetIp));
            }
        }
        else {
            targetIp = msclr::interop::marshal_as<std::string>(inputIp);
        }

        g_sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(PORT));
        inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr);

        if (connect(g_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            AddLog(L"Kết nối thất bại!");
            closesocket(g_sock);
            g_sock = INVALID_SOCKET;
            this->BeginInvoke(gcnew Action(this, &ChatForm::ResetConnectBtn));
            return;
        }

        isConnected = true;
        String^ authMsg = msclr::interop::marshal_as<String^>(AUTH_PREFIX) + txtUsername->Text;
        cli::array<unsigned char>^ authBytes = System::Text::Encoding::UTF8->GetBytes(authMsg);
        {
            pin_ptr<unsigned char> p = &authBytes[0];
            send(g_sock, (const char*)p, authBytes->Length, 0);
        }

        this->BeginInvoke(gcnew Action(this, &ChatForm::SwitchToChat));

        receiveThread = gcnew Thread(gcnew ThreadStart(this, &ChatForm::ReceiveLoop));
        receiveThread->Start();
    }

    void ResetConnectBtn() {
        btnConnect->Enabled = true;
        btnConnect->Text = L"KẾT NỐI SERVER";
    }

    void SwitchToChat() {
        loginPanel->Visible = false;
        chatPanel->Visible = true;
        lblOnlineStatus->Text = L"Đang trực tuyến: " + txtUsername->Text;
        rtbMessages->Clear();
    }

    void SwitchToLogin() {
        chatPanel->Visible = false;
        loginPanel->Visible = true;
        ResetConnectBtn();
    }

    void OnDisconnectClick(Object^ sender, EventArgs^ e) {
        isConnected = false;
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        SwitchToLogin();
    }

    void OnSendClick(Object^ sender, EventArgs^ e) {
        SendMessage();
    }

    void OnInputKeyDown(Object^ sender, KeyEventArgs^ e) {
        if (e->KeyCode == Keys::Enter) {
            SendMessage();
            e->SuppressKeyPress = true;
        }
    }

    void SendMessage() {
        String^ input = txtInput->Text;
        if (String::IsNullOrWhiteSpace(input)) return;

        String^ fullMsg = msclr::interop::marshal_as<String^>(MSG_PREFIX) + input;
        cli::array<unsigned char>^ bytes = System::Text::Encoding::UTF8->GetBytes(fullMsg);
        {
            pin_ptr<unsigned char> p = &bytes[0];
            send(g_sock, (const char*)p, bytes->Length, 0);
        }

        AddMessage(txtUsername->Text + ": " + input);
        txtInput->Clear();
    }
};

[STAThreadAttribute]
int main(cli::array<System::String^>^ args) {
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew ChatForm());
    return 0;
}
