/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
Socket.cpp
*/
#pragma once
#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Socket.h"

const int INITIAL_BUF_SIZE = 8192;
const int MAX_ROBOT_BUF = 16384;
const int MAX_PAGE_BUF = 2097152;

using namespace std;

//some code adapted from sample code (AFSC), will be marked with the following:
// **AFSC**

/**
* The default constructor for the Socket object, it initializes the class
*/
Socket::Socket() {

	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	// allocate this buffer once, then possibly reuse for multiple connections in Part 3
	buf = new char[INITIAL_BUF_SIZE]; // start with 8 KB

	allocatedSize = INITIAL_BUF_SIZE;
}

/**
* Reinitializes the socket fd to be used in connections, so the Socket object can be reused for multiple requests
* @returns {bool} returns true if successful, false if not
*/
bool Socket::Reinit() {
	//closesocket(sock);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// *AFSC* //
	if (sock == INVALID_SOCKET) {
		printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}
	return true;
}

/*
//used for previous parts of the HW, leaving it here in case I want to play with it later
DWORD Socket::DNSLookup(string host) {

	DWORD IP = inet_addr(host.c_str());
	if (IP == INADDR_NONE) {
		if ((remote = gethostbyname(host.c_str())) == NULL) {

			cout << " invalid host: not FQDM or IP address" << endl;
			return NULL;
		}
		else {
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
			return NULL;
		}
	}

	server.sin_addr.S_un.S_addr = IP;

	return IP;
}
*/
/*
//another function from old PA, leaving it here in case I want to use it later
IN_ADDR Socket::presend(ParsedURL url) {
	clock_t timer;
	double duration;

	if (!dns) {

		// *AFSC* //
		cout << "\t  Doing DNS...";

		//the timing code I adapted from stackoverflow
		//https://stackoverflow.com/questions/3220477/how-to-use-clock-in-c

		timer = clock();

		DWORD IP = DNSLookup(url.host);

		duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
		printf("done in %.1f ms, found %s\n", duration * 1000, inet_ntoa(server.sin_addr));

		if (IP != NULL)
			server.sin_addr.S_un.S_addr = IP;

		url.dns_res = server.sin_addr;
		url.dns = true;

	}

	return server.sin_addr;
}
*/

/**
* Sends the given request to the specified server address
* @param {ParsedURL} url: the ParsedURL object that represents the host as a URL
* @param {sockaddr_in} server: the sockaddr_in object that contains the DNS resolved IP address
* @param {string} request: the string representing the request to be sent to the host
* @returns {bool} returns true if the request was sent successfully, false if it was not
*/
bool Socket::SendRequest(ParsedURL url, sockaddr_in server, string request) {

	/*
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//// *AFSC* //
	if (sock == INVALID_SOCKET) {
		//printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	clock_t timer;
	double duration;
	*/

	server.sin_family = AF_INET;
	server.sin_port = htons(url.port);

	/*if (robots)
		cout << "\t  Connecting on robots... ";
	else
		cout << "\t* Connecting on page... ";

	timer = clock();*/

	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		//cout << "failed with " << WSAGetLastError() << endl;
		return false;
	}
	/*
	duration = (clock() - timer) / (double)CLOCKS_PER_SEC;

	printf("done in %.1f ms\n", duration * 1000);
	string request;

	if (robots)
		request = url.generateRobotRequest();
	else
		request = url.generateRequest();
	*/

	if (send(sock, request.c_str(), strlen(request.c_str()) + 1, 0) == SOCKET_ERROR) {
		//cout << "Send error: " << WSAGetLastError() << endl;
		return false;
	}
	return true;
}

/**
* Reads data available to the socket. The data is stored in the char* buf memeber of the Socket function
* @returns {bool} returns true if read was successful, false if not
*/
bool Socket::Read() {
	double duration;

	timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	int curPos = 0;
	fd_set readset;
	int ret;

	//cout << "\t  Loading...";

	clock_t timer = clock();

	while (true) {

		// wait to see if socket has any data (see MSDN)

		timeout.tv_sec -= floor(((clock() - timer) / (double)CLOCKS_PER_SEC));
		//timeout.tv_usec = 0;

		FD_ZERO(&readset);
		FD_SET(sock, &readset);

		if ((ret = select(1, &readset, 0, 0, &timeout)) > 0) {
			// new data available; now read the next segment
			int bytes = recv(sock, buf + curPos, allocatedSize - curPos, 0);
			if (bytes < 0) {
				//cout << WSAGetLastError() << endl;
				break;
			}
			if (bytes == 0) {
				// NULL-terminate buffer
				buf[curPos + 1] = '\0';
				//duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
				//printf("done in %.1f ms with %d bytes\n", duration * 1000, curPos);
				//closesocket(sock);
				return true; // normal completion
			}
			curPos += bytes; // adjust where the next recv goes
			if (allocatedSize - curPos < 1024) {
				// resize buffer; besides STL, you can use
				// realloc(), HeapReAlloc(), or memcpy the buffer
				// into a bigger array
				/*if (robots && allocatedSize == MAX_ROBOT_BUF) {
					cout << " robots header too large, aborting..." << endl;
					break;
				}
				else if (!robots && allocatedSize == MAX_PAGE_BUF) {
					cout << " page too large, aborting..." << endl;
					break;
				}*/
				char* newBuf = new char[allocatedSize * 2];
				memcpy(newBuf, buf, allocatedSize);
				allocatedSize *= 2;
				delete buf;
				buf = newBuf;
			}

		}
		else if (ret == 0) {
			//cout << "Connection timed out" << endl;
			//system("pause");
			break;
		}
		else {
			//cout << "connection error: " << WSAGetLastError() << endl;
			break;
		}
	}
	return false;
}