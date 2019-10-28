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

#include <WinSock2.h>
#include <string.h>
#include <string>
#include <ctime>

#define STATUS_OK			0 // no error
#define ALREADY_CONNECTED	1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED		2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME		3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND			4 // sendto() failed in kernel
#define TIMEOUT				5 // timeout after all retx attempts are exhausted
#define FAILED_RECV			6 // recvfrom() failed in kernel

// TODO: add headers that you want to pre-compile here

#endif //PCH_H
