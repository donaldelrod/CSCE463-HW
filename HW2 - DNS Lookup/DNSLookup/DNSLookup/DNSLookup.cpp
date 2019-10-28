// DNSLookup.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <string.h>
#include <string>
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Packet.h"
#include <vector>
#include <ctime>


/* DNS query types */
#define DNS_A 1			/* name -> IP */
#define DNS_NS 2		/* name server */
#define DNS_CNAME 5		/* canonical name */
#define DNS_PTR 12		/* IP -> name */
#define DNS_HINFO 13	/* host info/SOA */
#define DNS_MX 15		/* mail exchange */
#define DNS_AXFR 252	/* request for zone transfer */
#define DNS_ANY 255		/* all records */

#define MAX_DNS_SIZE 512
#define MAX_ATTEMPTS 3

using namespace std;

unsigned int queryType(string host) {
	DWORD IP = inet_addr(host.c_str());
	//if it is not an IP address, its type A
	if (IP == INADDR_NONE)
		return DNS_A;
	//otherwise, its type PTR
	return DNS_PTR;
}

vector<string> splitString(string s, string token) {
	vector<string> split;
	int ind = -1;
	while ((ind = s.find(token)) >= 0) {
		split.push_back(s.substr(0, ind));
		s = s.substr(ind+1);
	}
	split.push_back(s);
	return split;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		cout << "Not enough arguments input" << endl;
		cout << "Usage: DNSLookup.exe hostname/ip nameserver" << endl;
		return 1;
	}
	string host(argv[1]);
	string ns(argv[2]);

	vector<string> host_parts = splitString(host, ".");

	ushort txid = 1;

	uint q_type = queryType(host);

	Packet packet(txid, host_parts, q_type);

	printf("Lookup\t\t: %s\n", host.c_str());
	printf("Query\t\t: %s, type %d, TXID %#x\n", packet.lookup_string.c_str(), q_type, txid);
	printf("Server\t\t: %s\n", ns.c_str());
	printf("********************************");

	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port - htons(0);

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		printf("bind() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	struct sockaddr_in remote;
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_addr.S_un.S_addr = inet_addr(ns.c_str());
	remote.sin_port = htons(53);

	timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	fd_set readset;

	uint attempt_no = 0;

	uint packet_size = packet.getPacketSize();

	char* buf = (char*)malloc(packet_size);
	memset(buf, 0, packet_size);
	memcpy(buf, (uchar*)&packet.packet, packet_size);

	struct UDP_Packet* testPacket = (UDP_Packet*)buf;

	clock_t timer;
	double duration;
	bool succ = false;
	int rsp_size;

	while (attempt_no < MAX_ATTEMPTS) {
		timer = clock();

		printf("\nAttempt %d with %d bytes...", attempt_no, packet.getPacketSize());

		if (sendto(sock, (char*)(testPacket), packet_size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
			printf("sendto() generated error %d\n", WSAGetLastError());
			//WSACleanup();
			attempt_no++;
			//return 1;
			continue;
		}

		sockaddr_in response;
		int rsp_addr_size = sizeof(response);

		FD_ZERO(&readset);
		FD_SET(sock, &readset);

		int av = select(0, &readset, NULL, NULL, &timeout);

		if (av <= 0) {
			duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
			attempt_no++;
			printf("timeout in %.1f ms", duration * 1000);
			//assume it timed out
			continue;
		}
		char* rspbuf = new char[MAX_DNS_SIZE];
		memset(rspbuf, 0, MAX_DNS_SIZE);
		
		//int* rsp_addr_size = 0;
		if ((rsp_size = recvfrom(sock, rspbuf, MAX_DNS_SIZE, 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
			printf("recvfrom() generated error %d\n", WSAGetLastError());
			//WSACleanup();
			attempt_no++;
			continue;
			//return 1;
		}

		duration = (clock() - timer) / (double)CLOCKS_PER_SEC;

		if (response.sin_addr.S_un.S_addr != remote.sin_addr.S_un.S_addr || response.sin_port != remote.sin_port) {
			//response is bogus, print some error here
			continue;
		}

		struct UDP_Packet* npacket = (struct UDP_Packet*)rspbuf;

		packet.changePackets(*npacket, rsp_size);

		//packet.printPacket();
		succ = true;
		break;

		attempt_no++;

	}
	WSACleanup();

	if (!succ) {
		//didn't get it in 3 tries
		return 1;
	}

	printf("response in %.1f ms with %d bytes\n", duration * 1000, rsp_size);
	printf("\tTXID %#.4x, flags %#.4x, questions %d, answers %d, authority %d, additional %d\n",
		packet.getTXID(), packet.getFlags(), packet.getNQuestions(), packet.getNAnswers(), 
		packet.getNAuthority(), packet.getNAdditional());
	printf("\tsucceeded with Rcode = %d\n", packet.getResult());

	printf("\t------------ [questions] ----------\n");
	for (int i = 0; i < packet.answers.size(); i++) {
		printf("\t\t%s\n", packet.answers.at(i).c_str());
	}
    return 0;
}
