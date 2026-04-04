#include <iostream>
#include "Protocol.h"
using namespace std;

int main()
{
    cout << "PORT: " << PORT << endl;
    cout << "ENCODING: " << ENCODING << endl;
    cout << formatMessage("Duoc", "Hello");
    return 0;
}