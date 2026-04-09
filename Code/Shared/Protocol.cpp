#include "Protocol.h"
using namespace std;

string formatMessage(const string& username, const string& message)
{
    return "[" + username + "]: " + message + ENDLINE;
}
