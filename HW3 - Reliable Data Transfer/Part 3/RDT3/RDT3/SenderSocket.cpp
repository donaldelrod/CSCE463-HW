#include "pch.h"

#include "SenderSocket.h"

UINT SenderSocket::worker_thread(LPVOID w_params) {
	worker_params *params = (worker_params*) &w_params;

	int kernelBuffer = 20e6;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR) {
		printf("setsockopt rcvbuf error: %d\n", WSAGetLastError());
	}
	kernelBuffer = 20e6;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR) {
		printf("setsockopt sndbuf error: %d\n", WSAGetLastError());
	}
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	//make the socket trigger an event when it has data to read
	WSAEventSelect(sock, socketReceiveReady, FD_READ);

	int last_ack = 0;
	int num_dups = 0;
	int next_packet = 0;
	bool fast_retxed = false,
		sender_base_moved = false;

	double base_expires;

	HANDLE events[] = { socketReceiveReady, full };
	while (true) {
		DWORD timeout_millis = floor(rto * 1000);
		if (sender_base == next_packet)//no_packets_pending > 0) //sender base == next to send
			timeout_millis = packet_timer - clock();
		else if (base_expires < clock())
			timeout_millis = 0;
		else
			timeout_millis = INFINITE;
		int ret = WaitForMultipleObjects(2, events, false, timeout_millis);
		switch (ret) {
		case WAIT_TIMEOUT:
			//retx buf[senderBase % W];
			break;
		case 0: {
			int ack_no = recv_data_pckt(); // move senderBase; fast retx
			if (ack_no == sender_base) { //change to sender base
				num_dups++;
				if (num_dups == 3) {
					fast_retxed = true;
					//should fast retx here
					//retx buf[senderBase % W];
				}
				//continue;
			}
			else if (ack_no < sender_base) //ignore this packet
				break;
			else { //sender base moved forward
				//last_ack = ack_no;
				num_dups = 0;
				sender_base = ack_no;
				sender_base_moved = true;
			}
			break;
		}
		case 1:
			send_data_pckt(&pending_packets[next_packet % s_window]);
			//sendto(pending_packets[nextToSend % s_window].buf);
			next_packet++; //time exire = rto + current time
			break;
		default: //handle failed wait;
			printf("critical unknown error, error no %d, exiting...\n", WSAGetLastError());
			exit(1);
		}

		if ((next_packet == sender_base) || fast_retxed || sender_base_moved) { //first packet of window || 
			//recompute timerExpire
		}
	}
}


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
	s_window = senderWindow;

	sender_base = 0;

	pending_packets = (Packet*)malloc(sizeof(Packet)*s_window);

	current_seq_no = 0;
	dev_rtt = 0;

	timeouts = 0;
	fast_retransmits = 0;
	bytes_acked = 0;

	full = CreateSemaphore(NULL, 0, s_window, NULL);
	empty = CreateSemaphore(NULL, s_window, s_window, NULL);
	event_quit = CreateEvent(NULL, true, false, NULL);
	socketReceiveReady = CreateEvent(NULL, false, false, NULL);
	

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

	int err;

	while (attempt_no++ < MAX_SYN_ATTEMPTS) {
		since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;
		//printf(" [ %.3f] --> SYN 0 (attempt %d of 3, RTO 1) to %s\n", since_start, attempt_no, inet_ntoa(remote.sin_addr));
		if ((err = send_syn_pckt()) != STATUS_OK)
			continue;
		if ((err = recv_syn_pckt(1, 0)) == STATUS_OK) {
			open = true;
			break;
		}
	}

	return err;
}

int SenderSocket::Send(char* buf, unsigned int buf_size) {
	HANDLE arr[] = { event_quit, empty };
	WaitForMultipleObjects(2, arr, false, INFINITE);

	slot = current_seq_no % s_window;
	Packet *p = pending_packets + slot;

	p->type = DATA_PACKET;
	p->size = sizeof(SenderDataHeader) + buf_size;

	//add the packet sequence number to the Packet object
	p->seq = current_seq_no;

	struct SenderDataPacket* packet = new SenderDataPacket();

	//sets the sequence number in the actual packet
	packet->sdh.seq = current_seq_no;
	//copy the data into the actual data packet
	memcpy(packet->data, buf, buf_size);
	//copy the entire packet into the Packet object
	memcpy(p->buf, (char*)packet, sizeof(SenderDataHeader) + buf_size);

	current_seq_no++;

	ReleaseSemaphore(full, 1, NULL);

	return STATUS_OK;

	//int status, attempt_no = 0;
	//while (attempt_no++ < MAX_ATTEMPTS) {
	//	if ((status = send_data_pckt(buf, buf_size)) == STATUS_OK) {
	//		//printf("data packet sent successfully\n");
	//		if ((status = recv_data_pckt()) == STATUS_OK) {
	//			//printf("ack received successfully\n");
	//			return STATUS_OK;
	//		}
	//		else continue;
	//	}
	//	else continue;
	//}
	//return status;
}

int SenderSocket::Close() {
	int attempt_no = 0, err;
	double since_start;
	while (attempt_no++ < MAX_ATTEMPTS) {
		since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;
		//printf(" [ %.3f] --> FIN 0 (attempt %d of 5, RTO %.3f) to %s\n", since_start, attempt_no, rto, inet_ntoa(remote.sin_addr));
		if ((err = send_fin_pckt()) != STATUS_OK)
			continue;
		if ((err = recv_fin_pckt(floor(rto), ((rto - floor(rto)) * 1000000))) == STATUS_OK) {
			open = false;
			break;
		}
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
	packet->lp.bufferSize = s_window + 3;//buff_size;

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

	char* rspbuf = new char[sizeof(ReceiverHeader)];
	memset(rspbuf, 0, sizeof(ReceiverHeader));

	int rsp_size;
	sockaddr_in response;
	int rsp_addr_size = sizeof(response);

	if ((rsp_size = recvfrom(sock, rspbuf, sizeof(ReceiverHeader), 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
		printf(": recvfrom() generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return FAILED_RECV;
	}


	struct ReceiverHeader* resp_packet = (struct ReceiverHeader*)rspbuf;

	//printf("\nflags: %d\t%d\t%d\t%d\t%x\n", resp_packet->flags.reserved, resp_packet->flags.SYN, resp_packet->flags.ACK,
	//	resp_packet->flags.FIN, resp_packet->flags.magic);

	double packet_rtt = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;

	rto = packet_rtt * 3;
	est_rtt = packet_rtt;

	double since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;

	if (resp_packet->flags.SYN == 0x1 && resp_packet->flags.ACK == 0x1) {
		//printf(" [ %.3f] <-- SYN - ACK 0 window %d; setting initial RTO to %.4f\n", since_start, resp_packet->recvWnd, rto);
		recv_window = resp_packet->recvWnd;
		return STATUS_OK;
	}
	return -2;
}

int SenderSocket::send_fin_pckt() {
	struct SenderSynHeader* packet = new SenderSynHeader();
	packet->lp.RTT = rtt;

	packet->lp.speed = link_speed * (pow(10, 6));
	packet->lp.pLoss[0] = pckt_loss_f;
	packet->lp.pLoss[1] = pckt_loss_r;
	packet->lp.bufferSize = 200;// s_window + 3;//buff_size;

	packet->sdh.seq = current_seq_no;
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

	char* rspbuf = new char[sizeof(ReceiverHeader)];
	memset(rspbuf, 0, sizeof(ReceiverHeader));

	int rsp_size;
	sockaddr_in response;
	int rsp_addr_size = sizeof(response);

	if ((rsp_size = recvfrom(sock, rspbuf, sizeof(ReceiverHeader), 0, (SOCKADDR*)&response, &rsp_addr_size)) < 0) {
		printf(": recvfrom() generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return FAILED_RECV;
	}


	struct ReceiverHeader* resp_packet = (struct ReceiverHeader*)rspbuf;

	clock_t after_send = clock();

	double packet_rtt = (after_send - packet_timer) / (double)CLOCKS_PER_SEC;

	//rto = packet_rtt * 3;

	double since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;

	if (resp_packet->flags.FIN == 0x1 && resp_packet->flags.ACK == 0x1) {
		printf("[ %.3f] <-- FIN - ACK %d window %X\n", since_start, current_seq_no, resp_packet->recvWnd);
		return STATUS_OK;
	}
	return -1;
}

int SenderSocket::send_data_pckt(Packet* p) {//char* buf, unsigned int buf_size) {
	//struct SenderDataPacket* packet = new SenderDataPacket();

	//packet->sdh.seq = current_seq_no;
	//packet->sdh.flags.SYN = 0x1;

	//memcpy(packet->data, buf, buf_size);

	//printf("\nflags: %d\t%d\t%d\t%d\t%x\n", packet->sdh.flags.reserved, packet->sdh.flags.SYN, packet->sdh.flags.ACK,
	//	packet->sdh.flags.FIN, packet->sdh.flags.magic);

	//pending_packet(this packet_=>txTime = clock();

	p->txTime = clock();


	if (sendto(sock, p->buf, p->size, 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
		printf(": sendto() generated error %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	return STATUS_OK;
}

int SenderSocket::recv_data_pckt() {

	double duration;

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

	struct ReceiverHeader* resp_packet = (struct ReceiverHeader*)rspbuf;

	//printf("response packet sequence number: %d\n", resp_packet->ackSeq);

	//printf("current sequence number:         %d\n", current_seq_no+1);

	double packet_rtt = (clock() - packet_timer) / (double)CLOCKS_PER_SEC;

	//printf("packet_rtt: %.5f\n", packet_rtt);

	calculate_rto(packet_rtt);

	//printf("new rto:      %.5f\n", rto);

	double since_start = (clock() - begin_time) / (double)CLOCKS_PER_SEC;
	//resp_packet->flags.SYN == 0x1 && *9

	if (resp_packet->ackSeq >= sender_base && resp_packet->ackSeq <= sender_base + s_window) {
		return resp_packet->ackSeq;
	}


	//if (resp_packet->ackSeq == current_seq_no+1) {
	//	//printf(" [ %.3f] <-- DATA ACK %d window %d; setting initial RTO to %.4f\n", since_start, resp_packet->ackSeq, resp_packet->recvWnd, rto);
	//	recv_window = resp_packet->recvWnd;
	//	//current_seq_no++;
	//	bytes_acked += MAX_PKT_SIZE;
	//	return STATUS_OK;
	//}
	return -3;
}

void SenderSocket::calculate_est_rtt(float latest_sample) {
	float a = 0.125;
	//printf("old est_rtt: %.5f\n", est_rtt);
	est_rtt = (1 - a)*est_rtt + a * latest_sample;
	//printf("new est_rtt: %.5f\n", est_rtt);
}

void SenderSocket::calculate_dev_rtt(float latest_sample) {
	float b = 0.25;
	dev_rtt = (1 - b)*dev_rtt + b*abs(latest_sample - est_rtt);
}

void SenderSocket::calculate_rto(float latest_sample) {
	calculate_est_rtt(latest_sample);
	calculate_dev_rtt(latest_sample);
	rto = est_rtt + 4 * max(dev_rtt, 0.010);
	//printf("in calc_est_rtt:\n\tlatest sample: %.5f\n\tnew rto: %.5f\n\test_rtt: %.5f\n\tdev_rtt: %.5f\n",
	//	latest_sample, rto, est_rtt, dev_rtt);
}