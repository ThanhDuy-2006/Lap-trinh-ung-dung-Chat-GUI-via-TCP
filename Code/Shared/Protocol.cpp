#include "Protocol.h"
using namespace std;
const string ENCODING = "ASCII";
const string ENDLINE = "\n";

string formatMessage(const string& username, const string& message)
{
    return "[" + username + "]: " + message + ENDLINE;
}
