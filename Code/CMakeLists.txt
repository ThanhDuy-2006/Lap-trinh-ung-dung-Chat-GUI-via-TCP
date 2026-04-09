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
    # Remove flags incompatible with /clr
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Shared Protocol Library (for CLI versions)
add_library(chat_protocol STATIC
  Shared/Protocol.cpp
)
target_include_directories(chat_protocol PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/Shared"
)

find_package(Threads REQUIRED)

# --- CLI SERVER ---
add_executable(server Server/server.cpp)
target_compile_options(server PRIVATE "/EHsc")
target_link_libraries(server PRIVATE chat_protocol ws2_32 Threads::Threads)

# --- CLI CLIENT ---
add_executable(client Client/client.cpp)
target_compile_options(client PRIVATE "/EHsc")
target_link_libraries(client PRIVATE chat_protocol ws2_32 Threads::Threads)

# --- GUI CLIENT (WINFORMS) ---
add_executable(client_gui WIN32 
    "Gui/GuiClient/gui_client.cpp"
    Shared/Protocol.cpp
)
set_target_properties(client_gui PROPERTIES 
    COMMON_LANGUAGE_RUNTIME ""
    VS_DOTNET_REFERENCES "System;System.Drawing;System.Windows.Forms"
    LINK_FLAGS "/ENTRY:main /SUBSYSTEM:WINDOWS"
)
if(MSVC)
    target_compile_options(client_gui PRIVATE "/clr" "/EHa")
    # For Ninja generator, we must force using the assemblies
    target_compile_options(client_gui PRIVATE "/FUSystem.dll" "/FUSystem.Drawing.dll" "/FUSystem.Windows.Forms.dll")
endif()
target_include_directories(client_gui PRIVATE Shared)
target_link_libraries(client_gui PRIVATE ws2_32)

# --- GUI SERVER (WINFORMS) ---
add_executable(server_gui WIN32 
    "Gui/Guiserver/gui_server.cpp"
    Shared/Protocol.cpp
)
set_target_properties(server_gui PROPERTIES 
    COMMON_LANGUAGE_RUNTIME ""
    VS_DOTNET_REFERENCES "System;System.Drawing;System.Windows.Forms"
    LINK_FLAGS "/ENTRY:main /SUBSYSTEM:WINDOWS"
)
if(MSVC)
    target_compile_options(server_gui PRIVATE "/clr" "/EHa")
    target_compile_options(server_gui PRIVATE "/FUSystem.dll" "/FUSystem.Drawing.dll" "/FUSystem.Windows.Forms.dll")
endif()
target_include_directories(server_gui PRIVATE Shared)
target_link_libraries(server_gui PRIVATE ws2_32)
