#pragma once
#include <string>
using namespace std;
// TCP port; client and server must match (was split: 8888 vs 9050).
static constexpr int PORT = 9050;
// UDP discovery for finding server in the same LAN.
static constexpr int DISCOVERY_PORT = 9051;
static constexpr const char* DISCOVERY_REQUEST = "CHAT_DISCOVER_REQUEST";
static constexpr const char* DISCOVERY_RESPONSE = "CHAT_DISCOVER_RESPONSE";
static constexpr const char* AUTH_PREFIX = "AUTH|";
static constexpr const char* MSG_PREFIX = "MSG|";
static constexpr const char* FROM_PREFIX = "FROM|";
static constexpr const char* SYSTEM_PREFIX = "SYSTEM|";
static constexpr int MAX_USERNAME_LENGTH = 24;
extern const string ENCODING;
extern const string ENDLINE;
string formatMessage(const std::string& username, const std::string& message);
