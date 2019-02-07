/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
*/
#pragma once
#include <iostream>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "ParsedURL.h"
#include <ctime>

class Socket {
public:
	SOCKET sock; // socket handle
	char *buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; // current position in buffer
	int currentBufferSize;

	struct hostent *remote;
	struct sockaddr_in server;

	Socket();
	bool Read(void);
	bool SendRequest(ParsedURL url);
};