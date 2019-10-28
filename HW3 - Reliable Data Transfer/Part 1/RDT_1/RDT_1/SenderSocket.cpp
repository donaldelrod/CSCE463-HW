#include "pch.h"

#include "SenderSocket.h"


int SenderSocket::Open(string host_n, int port_no_n, UINT64 buff_s_n, int senderWindow, float link_n, float rtt_n, float packet_loss_f, float packet_loss_r) {

	if (open)
		return ALREADY_CONNECTED;
	
	begin_time = clock();

	host = host_n;
	port_no = port_no_n;
	rtt = rtt_n;
	pckt_loss_f = packet_loss_f;
	pckt_loss_r = packet_loss_f;
	link_speed = link_n;
	buff_size = buff_s_n;

	//set up socket
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//bind socket to port
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		printf("bind() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//set up remote sockaddr_in struct
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;

	DWORD IP = inet_addr(host.c_str());
	if (IP == INADDR_NONE) {
		struct hostent *r;
		if ((r = gethostbyname(host.c_str())) == NULL) {
			//printf(": not valid host\n");
			return INVALID_NAME;
		}
		else {
			memcpy((char*)&(remote.sin_addr), r->h_addr, r->h_length);
			//printf("copied ip from gethostbyname\n");
		}
	} else remote.sin_addr.S_un.S_addr = IP;

	//remote.sin_addr.S_un.S_addr = inet_addr(host.c_str());
	remote.sin_port = htons(port_no);

	int attempt_no = 0;
	//clock_t timer = clock();
	double since_start;

	bool succ = false;

	int err;

	while (attempt_no++ < MAX_SYN_ATTEMPTS) {
		since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;
		printf(" [ %.3f] --> SYN 0 (attempt %d of 3, RTO 1) to %s\n", since_start, attempt_no, inet_ntoa(remote.sin_addr));
		if ((err = send_syn_pckt()) != STATUS_OK)
			continue;
		if ((err = recv_syn_pckt(1, 0)) == STATUS_OK) {
			succ = true;
			break;
		}
	}

	//if (succ)
	//	return STATUS_OK;
	return err;
}

int SenderSocket::Send(char* buf, unsigned int buf_size) {
	return 0;
}

int SenderSocket::Close() {
	int attempt_no = 0, err;
	double since_start;
	while (attempt_no++ < MAX_ATTEMPTS) {
		since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;
		printf(" [ %.3f] --> FIN 0 (attempt %d of 5, RTO %.3f) to %s\n", since_start, attempt_no, rto, inet_ntoa(remote.sin_addr));
		if ((err = send_fin_pckt()) != STATUS_OK)
			continue;
		if ((err = recv_fin_pckt(floor(rto), ((rto - floor(rto)) * 1000000))) == STATUS_OK)
			break;
	}

	return err;
}

int SenderSocket::send_syn_pckt() {
	
	struct SenderSynHeader* packet = new SenderSynHeader();

	//setup stuff
	packet->lp.RTT = rtt;
	
	packet->lp.speed = link_speed * (pow(10, 6));
	packet->lp.pLoss[0] = pckt_loss_f;
	packet->lp.pLoss[1] = pckt_loss_r;
	packet->lp.bufferSize = 24;// s_window + 3;//buff_size;

	packet->sdh.seq = 0;
	packet->sdh.flags.SYN = 0x1;
	
	//printf("\nflags: %d\t%d\t%d\t%d\t%x\n", packet->sdh.flags.reserved, packet->sdh.flags.SYN, packet->sdh.flags.ACK,
	//	packet->sdh.flags.FIN, packet->sdh.flags.magic);

	packet_timer = clock();

	//printf("\nrtt: %g\tspeed: %g\tploss[0]: %g\tploss[1]: %g\tbuffersize: %d\tsizeof: %d\n", rtt, link_speed * pow(10, 6),
	//	pckt_loss_f, pckt_loss_r, 200, sizeof(SenderSynHeader));
	
	if (sendto(sock, (char*)packet, sizeof(SenderSynHeader), 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
		printf(": sendto() generated error %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	return STATUS_OK;
}

int SenderSocket::recv_syn_pckt(long to_sec, long to_usec) {
	timeval timeout;
	timeout.tv_sec = to_sec;
	timeout.tv_usec = to_usec;

	double duration;


	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(sock, &readset);

	int av = select(0, &readset, NULL, NULL, &timeout);

	if (av <= 0) {
		duration = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;
		//printf(": timeout in %.1f ms\n", duration * 1000);
		//assume it timed out
		return TIMEOUT;
	}

	char* rspbuf = new char[sizeof(SenderSynHeader)];
	memset(rspbuf, 0, sizeof(SenderSynHeader));

	int rsp_size;
	sockaddr_in response;
	int rsp_addr_size = sizeof(response);

	if ((rsp_size = recvfrom(sock, rspbuf, sizeof(SenderSynHeader), 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
		printf(": recvfrom() generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return FAILED_RECV;
	}


	duration = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;

	rto = (float)duration * 3000;

	struct ReceiverHeader* resp_packet = (struct ReceiverHeader*)rspbuf;

	//printf("\nflags: %d\t%d\t%d\t%d\t%x\n", resp_packet->flags.reserved, resp_packet->flags.SYN, resp_packet->flags.ACK,
	//	resp_packet->flags.FIN, resp_packet->flags.magic);

	double packet_rtt = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;

	rto = packet_rtt * 3;

	double since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;

	if (resp_packet->flags.SYN == 0x1 && resp_packet->flags.ACK == 0x1) {
		printf(" [ %.3f] <-- SYN - ACK 0 window %d; setting initial RTO to %.4f\n", since_start, resp_packet->recvWnd, rto);
		recv_window = resp_packet->recvWnd;
		return STATUS_OK;
	}
	return -1;
}

int SenderSocket::send_fin_pckt() {
	struct SenderSynHeader* packet = new SenderSynHeader();
	packet->lp.RTT = rtt;

	packet->lp.speed = link_speed * (pow(10, 6));
	packet->lp.pLoss[0] = pckt_loss_f;
	packet->lp.pLoss[1] = pckt_loss_r;
	packet->lp.bufferSize = 200;// s_window + 3;//buff_size;

	packet->sdh.seq = 0;
	packet->sdh.flags.FIN = 0x1;

	//printf("\nflags: %d\t%d\t%d\t%d\t%x\n", packet->sdh.flags.reserved, packet->sdh.flags.SYN, packet->sdh.flags.ACK,
	//	packet->sdh.flags.FIN, packet->sdh.flags.magic);

	packet_timer = clock();

	//printf("\nrtt: %g\tspeed: %g\tploss[0]: %g\tploss[1]: %g\tbuffersize: %d\tsizeof: %d\n", rtt, link_speed * pow(10, 6),
	//	pckt_loss_f, pckt_loss_r, 200, sizeof(SenderSynHeader));

	if (sendto(sock, (char*)packet, sizeof(SenderSynHeader), 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
		printf(": sendto() generated error %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	return STATUS_OK;
}

int SenderSocket::recv_fin_pckt(long to_sec, long to_usec) {
	timeval timeout;
	timeout.tv_sec = to_sec;
	timeout.tv_usec = to_usec;

	double duration;


	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(sock, &readset);

	int av = select(0, &readset, NULL, NULL, &timeout);

	if (av <= 0) {
		duration = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;
		//printf(": timeout in %.1f ms\n", duration * 1000);
		//assume it timed out
		return TIMEOUT;
	}

	char* rspbuf = new char[sizeof(SenderSynHeader)];
	memset(rspbuf, 0, sizeof(SenderSynHeader));

	int rsp_size;
	sockaddr_in response;
	int rsp_addr_size = sizeof(response);

	if ((rsp_size = recvfrom(sock, rspbuf, sizeof(SenderSynHeader), 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
		printf(": recvfrom() generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return FAILED_RECV;
	}

	duration = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;

	rto = (float)duration * 3000;

	struct ReceiverHeader* resp_packet = (struct ReceiverHeader*)rspbuf;

	clock_t after_send = clock();

	double packet_rtt = (after_send - packet_timer) / (double)CLOCKS_PER_SEC;

	rto = packet_rtt * 3;

	double since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;

	if (resp_packet->flags.FIN == 0x1 && resp_packet->flags.ACK == 0x1) {
		printf(" [ %.3f] <-- FIN - ACK 0 window %d\n", since_start, resp_packet->recvWnd);
		return STATUS_OK;
	}
	return -1;
}