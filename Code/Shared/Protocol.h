#pragma once
#include <string>
using namespace std;
/** Cổng TCP — client và server dùng cùng giá trị (trước đây client/server dùng 8888, Protocol lại 9050). */
static constexpr int PORT = 9050;
extern const string ENCODING;
extern const string ENDLINE;
string formatMessage(const std::string& username, const std::string& message);
