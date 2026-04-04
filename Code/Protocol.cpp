#include "Protocol.h"
using namespace std;
const int PORT = 9050;
const string ENCODING = "ASCII";
const string ENDLINE = "\n";

string formatMessage(const string& username, const string& message)
{
    return "[" + username + "]: " + message + ENDLINE;
}