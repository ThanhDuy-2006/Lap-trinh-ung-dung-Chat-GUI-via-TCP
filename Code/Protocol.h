#pragma once
#include <string>
using namespace std;
extern const int PORT;
extern const string ENCODING;
extern const string ENDLINE;
string formatMessage(const std::string& username, const std::string& message);