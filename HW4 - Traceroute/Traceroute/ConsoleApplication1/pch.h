// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

#ifndef PCH_H
#define PCH_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <string>
#include <ctime>
#include <vector>

#include "Checksum.h"

#endif //PCH_H
