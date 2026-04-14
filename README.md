# UDM_08 – Lập trình ứng dụng Chat (GUI) via TCP

## Mô tả

Dự án xây dựng **ứng dụng chat sử dụng giao thức TCP** với **giao diện đồ họa (GUI)**.
Toàn bộ các chức năng của chương trình được thực hiện thông qua GUI.

## Chức năng

* Toàn bộ chức năng liên quan đến ứng dụng đều thực hiện qua GUI

## Cấu trúc dự án

```
MyProjectClone/
├── Code/                   ← Mã nguồn chương trình
│   ├── Client/             ← Mã nguồn Client (GUI WinForms)
│   │   └── client.cpp
│   ├── Server/             ← Mã nguồn Server (GUI WinForms)
│   │   └── server.cpp
│   ├── Shared/             ← Thư viện dùng chung (Protocol)
│   │   ├── Protocol.h
│   │   └── Protocol.cpp
│   ├── CMakeLists.txt
│   └── CMakePresets.json
├── DOCX/                   ← Báo cáo dự án (Word DOC/DOCX)
├── Extra/                  ← Tài liệu bổ sung, hình ảnh, proof
├── PPTX/                   ← Slide thuyết trình (PowerPoint PPT/PPTX)
├── README.md
└── .gitignore
```

## Yêu cầu & build (Windows)

Dự án dùng **Winsock + C++/CLI WinForms** — chỉ build trên **Windows** với **MSVC**.

Cần cài: **CMake** (≥ 3.16) và **Visual Studio 2022** (workload *Desktop development with C++*).

**Quan trọng:** *File → Open → Folder…* phải mở thư mục **`Code`** (nơi có `CMakeLists.txt`), **không** mở cả repo gốc nếu bạn dùng CMake trong Visual Studio / VS Code.

### Visual Studio

1. *File → Open → Folder…* → chọn thư mục **`Code`**.
2. Visual Studio nhận CMake; chọn target **server** hoặc **client** rồi *Build*.
3. Chạy: `server.exe` / `client.exe` trong `Code/build/...`.

### Dòng lệnh (CMake)

Trong terminal, `cd` vào **`Code`** rồi:

```text
cd Code
cmake --preset vs2022
cmake --build build --config Release
```

File thực thi nằm trong `Code/build/Release|Debug/`.

### Chạy thử

1. Terminal 1: chạy **server** — lắng nghe cổng mặc định **9050**, tự phát hiện LAN qua UDP **9051**.
2. Terminal 2: chạy **client**, nhập tên hiển thị và kết nối. Client tự tìm server trong LAN.
