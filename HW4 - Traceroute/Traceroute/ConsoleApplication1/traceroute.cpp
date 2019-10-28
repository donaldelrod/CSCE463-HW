// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>

#define WSA_STARTUP_ERR 0
#define ARG_ERR 1
#define SOCKET_ERR 2
#define GHBN_ERR 3
#define SETSOCKOPT_ERR 4
#define SENDTO_ERR 5
#define RECVFROM_ERR 6

#define IP_HDR_SIZE 20		/* RFC 791 */
#define ICMP_HDR_SIZE 8		/* RFC 792 */

#define MAX_RETRIES 3
#define N 30

/* max payload size of an ICMP message originated in the program */
#define MAX_SIZE 65200		/* max size of an IP datagram */

#define MAX_ICMP_SIZE (MAX_SIZE + ICMP_HDR_SIZE)
	/* the returned ICMP message will most likely include only 8 bytes
	* of the original message plus the IP header (as per RFC 792); however,
	* longer replies (e.g., 68 bytes) are possible */
#define MAX_REPLY_SIZE (IP_HDR_SIZE + ICMP_HDR_SIZE + MAX_ICMP_SIZE)
	/* ICMP packet types */

#define ICMP_ECHO_REPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_TTL_EXPIRED 11
#define ICMP_ECHO_REQUEST 8

#define IDENTIFIER 12345

using namespace std;

static u_short current_seq = 0;

u_long local_ip;
struct sockaddr_in remote;

/* remember the current packing state */
#pragma pack (push, 1)


/* define the IP header (20 bytes) */
class IPHeader {
public:
	u_char h_len : 4;		/* lower 4 bits: length of the header in dwords */
	u_char version : 4;		/* upper 4 bits: version of IP, i.e., 4 */
	u_char tos;				/* type of service (TOS), ignore */
	u_short len;			/* length of packet */
	u_short ident;			/* unique identifier */
	u_short flags;			/* flags together with fragment offset - 16 bits */
	u_char ttl;				/* time to live */
	u_char proto;			/* protocol number (6=TCP, 17=UDP, etc.) */
	u_short checksum;		/* IP header checksum */
	u_long source_ip;
	u_long dest_ip;
};

/* define the ICMP header (8 bytes) */
class ICMPHeader {
public:
	u_char type;			/* ICMP packet type */
	u_char code;			/* type subcode */
	u_short checksum;		/* checksum of the ICMP */
	u_short id;				/* application-specific ID */
	u_short seq;			/* application-specific sequence */
};
/* now restore the previous packing state */

class Packet {
public:
	//IPHeader iphead;
	ICMPHeader icmphead;

	Packet() {
		//memset(&iphead, 0, sizeof(IPHeader));
		memset(&icmphead, 0, sizeof(ICMPHeader));
	}

	Packet(u_char ttl) {
		//zero ip header, then set the ip header fields
		//memset(&iphead, 0, sizeof(IPHeader));
		//iphead.h_len = IP_HDR_SIZE;
		//iphead.version = 4;
		//iphead.tos = 0;
		//iphead.len = IP_HDR_SIZE + ICMP_HDR_SIZE;
		//iphead.ident = htons(IDENTIFIER);
		//iphead.flags = 0;
		//iphead.ttl = ttl;
		//iphead.proto = 17;//17 is udp
		//iphead.source_ip = local_ip;
		//iphead.dest_ip = d_ip;
		//iphead.checksum = 0;
		
		//zero the icmp header, then set the icmp header fields
		memset(&icmphead, 0, sizeof(ICMPHeader));
		icmphead.type = ICMP_ECHO_REQUEST;
		icmphead.code = 0;// 11;
		icmphead.id = htons(IDENTIFIER);
		icmphead.seq = htons(current_seq++);
		icmphead.checksum = 0;

		//printf("icmphead type: %d\n", icmphead.type);

		//now that all the data is set, we can calculate the checksums 
		//for both headers
		CalcChecksums();
	}

	void CalcChecksums() {
		Checksum cs;
		icmphead.checksum = cs.ip_checksum((u_short*)&icmphead, sizeof(ICMPHeader));
		//iphead.checksum = cs.ip_checksum((u_short*)&iphead, sizeof(IPHeader) + sizeof(ICMPHeader));
	}

	Packet(char* raw) {

	}
};
#pragma pack (pop) 

void handle_error(int err_code) {
	switch (err_code) {
	case WSA_STARTUP_ERR:
		printf("WSAStartup error %d\n", WSAGetLastError());
		break;
	case ARG_ERR:
		printf("To use this program, give either an IP address or a hostname as an argument to the program\n");
		printf("Example: traceroute.exe www.google.com\n");
		exit(1);
		break;
	case SOCKET_ERR:
		printf("socket() generated error %d\n", WSAGetLastError());
		break;
	case GHBN_ERR:
		printf("error: not valid host\n");
		break;
	case SETSOCKOPT_ERR:
		printf("setsockopt failed with %d\n", WSAGetLastError());
		break;
	case SENDTO_ERR:
		printf("sendto failed with %d\n", WSAGetLastError());
		break;
	case RECVFROM_ERR:
		printf("recvfrom() generated error %d\n", WSAGetLastError());
		break;
	default:
		printf("unknown error detected, exiting...\n");
		break;
	}

	WSACleanup();
	exit(-1);
}

//used to keep the packet with the data recorded about it
class PacketWrapper {
public:
	ICMPHeader icmphead;
	//Packet* p;
	clock_t sent_time, recv_time;
	u_char attempts;
	double rtt, rto;

	bool response, dns_done;

	int packet_ttl;
	u_long dns_ip;
	string resolved_hostname;

	HANDLE dns_thread_handle, done;
	//struct dns_params dns_p;

	char buffer[sizeof(IPHeader) + sizeof(ICMPHeader)];

	PacketWrapper() {
		attempts = 0;
		rtt = 0;
		rto = 0.5;
		response = false;

		dns_done = false;
		dns_ip = 0;
		resolved_hostname = "";
		packet_ttl = 0;

	};

	PacketWrapper(u_char ttl) {

		memset(&icmphead, 0, sizeof(ICMPHeader));
		icmphead.type = ICMP_ECHO_REQUEST;
		icmphead.code = 0;
		icmphead.id = htons(GetCurrentProcessId());//IDENTIFIER);
		icmphead.seq = htons(current_seq++);
		icmphead.checksum = 0;

		Checksum cs;
		icmphead.checksum = cs.ip_checksum((u_short*)&icmphead, sizeof(ICMPHeader));

		packet_ttl = ttl;
		memset(buffer, 0, sizeof(IPHeader) + sizeof(ICMPHeader));

		memcpy(buffer, &icmphead, sizeof(ICMPHeader));
		attempts = 0;
		rtt = 0;
		rto = 0.5;
		response = false;

		dns_done = false;
		dns_ip = 0;
		resolved_hostname = "";

		done = CreateEvent(NULL, false, false, NULL);
	}

	void startTimer() {
		attempts++;
		sent_time = clock();
	}

	void stopTimer() {
		recv_time = clock();
		rtt = (recv_time - sent_time) / (double)CLOCKS_PER_SEC;
		response = true;
	}

	void printPacketInfo() {
		printf("");
	}

	void transmit_packet(SOCKET sock) {

		int ttl = packet_ttl;

		if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
			closesocket(sock);
			handle_error(SETSOCKOPT_ERR);
		}
		startTimer(); //start packet timer and increment attempt no for packet
		if (sendto(sock, buffer, sizeof(ICMPHeader), 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
			printf("%d sendto failed with %d\n", ttl, WSAGetLastError());
			//closesocket(sock);
			handle_error(SENDTO_ERR);
		}
	}

	void retransmit_packet(SOCKET sock, double new_rto) {
		rto = new_rto;
		transmit_packet(sock);
	}

};


UINT WINAPI dns_thread(LPVOID params) {
	PacketWrapper* pw = (PacketWrapper*)params;

	hostent* reverse;

	printf("getting host by addr...");

	reverse = gethostbyaddr((char*)&(pw->dns_ip), 4, AF_INET);

	printf("done with %s\n", reverse->h_name);

	pw->resolved_hostname = reverse->h_name;
	pw->dns_done = true;

	int ret = SetEvent(pw->done);
	printf("setevent ret val: %d\n", ret);

	return 0;
}



int main(int argc, char** argv) {
	if (argc != 2)
		handle_error(ARG_ERR);

	string host(argv[1]);

	//set up socket
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0)
		handle_error(WSA_STARTUP_ERR);

	SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

	if (sock == INVALID_SOCKET)
		handle_error(SOCKET_ERR);


	DWORD IP = inet_addr(host.c_str());

	if (IP == INADDR_NONE) {
		struct hostent *r;

		if ((r = gethostbyname(host.c_str())) == NULL)
			handle_error(GHBN_ERR);
		else {
			memcpy((char*)&(remote.sin_addr), r->h_addr, r->h_length);
			//printf("copied ip from gethostbyname\n");
		}
	}
	else remote.sin_addr.S_un.S_addr = IP;

	IP = remote.sin_addr.S_un.S_addr;

	remote.sin_family = AF_INET;
	//array of packets to send
	PacketWrapper packets[N];


	//vector<PacketWrapper> outstanding, received;

	//create all the packets prior to sending them out
	for (int i = 0; i < N; i++)
		packets[i] = PacketWrapper(i);

	clock_t begin_send = clock();

	//send all of the packets
	for (int i = 0; i < N; i++) {
		packets[i].transmit_packet(sock);
	}

	int current_hop = 0;
	vector<HANDLE> dns_lookups;

	//give the socket an event to trigger when it receives a packet
	//doing it this way so we can switch between socket receives and DNS responses
	HANDLE socket_event = CreateEvent(NULL, false, false, NULL);
	WSAEventSelect(sock, socket_event, FD_READ);
	int n = 0;
	HANDLE events[N + 1];

	for (int i = 0; i < N; i++)
		events[i] = packets[i].done;

	events[N] = socket_event;

	while (1) {

		//printf("while loop\n");
		
		//check for timeout

		int ret = WaitForMultipleObjects(N + 1, events, false, INFINITE);
		if (ret == N) {//then the socket is ready to be read from
			printf("socket is ready to receive...\n");
			double duration;

			/*current_elapsed = (clock() - begin_send) / (double)CLOCKS_PER_SEC;*/

			char* rspbuf = new char[MAX_REPLY_SIZE];
			memset(rspbuf, 0, MAX_REPLY_SIZE);

			int rsp_size;
			sockaddr_in response;
			int rsp_addr_size = sizeof(response);


			if ((rsp_size = recvfrom(sock, rspbuf, MAX_REPLY_SIZE, 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
				handle_error(RECVFROM_ERR);
			}
			IPHeader* firsthead = (IPHeader*)rspbuf;
			printf("received a packet with size %d\n", ntohs(firsthead->len));
			if (rsp_size >= 56) {
				printf("received literallly any packet\n");
				IPHeader* newiphead = (IPHeader*)rspbuf;
				ICMPHeader* newicmphead = (ICMPHeader*)(newiphead + 1);
				if (newicmphead->type == ICMP_TTL_EXPIRED && newicmphead->code == 0) {
					printf("received a ttl expired packet");
					IPHeader* origiphead = (IPHeader*)(newicmphead + 1);
					ICMPHeader* origicmphead = (ICMPHeader*)(origiphead + 1);
					//sockaddr_in print;
					//memcpy(&print.sin_addr.S_un.S_addr, &(newiphead->source_ip), 4);
					//print.sin_family = AF_INET;

					printf("\nnew_source: %X\tnew_dest: %X\norig_source: %X\torig_dest: %X\n", newiphead->source_ip, newiphead->dest_ip,
						origiphead->source_ip, origiphead->dest_ip);


					struct in_addr paddr;
					u_int *ip = (u_int*)&(newiphead->source_ip);
					paddr.S_un.S_addr = *ip;
					char *ip_char = inet_ntoa(paddr);

					printf("\nnewiphead:\n\tidentifier:%d\n\toriginal ttl:%d\n\tsource ip of response:%s\n", ntohs(newiphead->ident), origiphead->ttl, ip_char);
					printf("\norigicmphead:\n\tidentifier:%d\n\ttype:%d\n\tsequence:%d\n", ntohs(origicmphead->id), origicmphead->type, origicmphead->seq);
					if (origiphead->proto == 1 && ntohs(origicmphead->id) == GetCurrentProcessId()) {//IDENTIFIER) {
						printf(", and it was a response to our packet\n");
						PacketWrapper rec = packets[origicmphead->seq];
						rec.stopTimer();
						rec.dns_ip = newiphead->source_ip;//.dns_p = dns_params(newiphead->source_ip);
						//event for letting this thread know if there is a dns response ready
						//start thread here
						rec.dns_thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dns_thread, &rec, 0, NULL);
					}
					else printf(", but it was not from this program\n");
				}
			}
			else {
				printf("received an unwanted packet: %d\n", n++);
				IPHeader* newiphead = (IPHeader*)rspbuf;
				printf("\nnew_source: %X\tnew_dest: %X\n", newiphead->source_ip, newiphead->dest_ip);
			}
			delete rspbuf;
		}
		else {//one of the DNS queries returned
			printf("packet %d returned from dns query with %s\n", ret, packets[ret].resolved_hostname);
			/*WaitForSingleObject(packets[ret].dns_thread_handle, INFINITE);
			CloseHandle(packets[ret].dns_thread_handle);
			printf("Closed dns thread %d\n", ret);*/
		}
		double time_diff;
		for (int i = 0; i < N; i++) {
			if (i == current_hop && packets[i].dns_done) {
				printf("current hop\n");
				in_addr print;
				print.S_un.S_addr = packets[i].dns_ip;
				printf("%2d  %s (%s)\t%.3f ms (%d)\n", current_hop,
					packets[i].resolved_hostname.c_str(), inet_ntoa(print),
					packets[i].rtt, packets[i].attempts);
				current_hop++;
				
			}
			if (packets[i].response)
				continue;
			time_diff = (clock() - packets[i].sent_time) / (double)CLOCKS_PER_SEC;
			if (time_diff > packets[i].rto) {
				printf("retrasmitting packet %d\n", i);
				double new_rto = 0.5;
				//we can take double the average of either side
				if (i != 0 && packets[i-1].response && 
					i != N-1 && packets[i+1].response) {
					double prev_rtt = packets[i - 1].rtt,
						next_rtt = packets[i + 1].rtt;

					new_rto = prev_rtt + next_rtt;
				}
				//we can double.5 the previous packets rtt
				else if (i != 0 && packets[i - 1].response) {
					new_rto = 2.5 * packets[i - 1].rtt;
				}
				//we can slightly increase the next packets rtt
				else if (i != N - 1 && packets[i + 1].response) {
					new_rto = 1.5 * packets[i + 1].rtt;
				}
				//otherwise we have nothing to go off and we set to 500ms

				packets[i].retransmit_packet(sock, new_rto);
			}
		}
		
	}

	//now we deal with receiving
	closesocket(sock);
	WSACleanup();
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu
