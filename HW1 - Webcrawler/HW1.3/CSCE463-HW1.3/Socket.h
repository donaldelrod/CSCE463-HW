/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
Socket.h
*/
#pragma once
#include "pch.h"
#include <iostream>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "ParsedURL.h"
#include <ctime>
#include <unordered_set>

class Socket {
public:
	SOCKET sock; // socket handle
	char *buf; // current buffer
	int allocatedSize; // bytes allocated for buf
	int curPos; // current position in buffer

	//struct hostent *remote;
	//struct sockaddr_in server;

	Socket();
	bool Read();
	bool SendRequest(ParsedURL url, sockaddr_in server, string request);
	bool Reinit();
	//DWORD DNSLookup(string host);
	//IN_ADDR presend(ParsedURL url);
};