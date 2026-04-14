cmake_minimum_required(VERSION 3.16)
project(ChatTcpGui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "This project uses Winsock and only supports Windows builds.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# --- CLIENT (GUI WinForms) ---
add_executable(client WIN32
    "Client/client.cpp"
    Shared/Protocol.cpp
)
set_target_properties(client PROPERTIES
    COMMON_LANGUAGE_RUNTIME ""
    VS_DOTNET_REFERENCES "System;System.Drawing;System.Windows.Forms"
    LINK_FLAGS "/ENTRY:main /SUBSYSTEM:WINDOWS"
)
if(MSVC)
    target_compile_options(client PRIVATE "/clr" "/EHa")
    target_compile_options(client PRIVATE "/FUSystem.dll" "/FUSystem.Drawing.dll" "/FUSystem.Windows.Forms.dll")
endif()
target_include_directories(client PRIVATE Shared)
target_link_libraries(client PRIVATE ws2_32)

# --- SERVER (GUI WinForms) ---
add_executable(server WIN32
    "Server/server.cpp"
    Shared/Protocol.cpp
)
set_target_properties(server PROPERTIES
    COMMON_LANGUAGE_RUNTIME ""
    VS_DOTNET_REFERENCES "System;System.Drawing;System.Windows.Forms"
    LINK_FLAGS "/ENTRY:main /SUBSYSTEM:WINDOWS"
)
if(MSVC)
    target_compile_options(server PRIVATE "/clr" "/EHa")
    target_compile_options(server PRIVATE "/FUSystem.dll" "/FUSystem.Drawing.dll" "/FUSystem.Windows.Forms.dll")
endif()
target_include_directories(server PRIVATE Shared)
target_link_libraries(server PRIVATE ws2_32)
