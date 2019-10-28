#pragma once

#include "pch.h"

#define MAGIC_PORT 22345
#define MAX_PKT_SIZE (1500-28)

#define MAX_SYN_ATTEMPTS 3
#define MAX_ATTEMPTS 50

#define FORWARD_PATH 0
#define RETURN_PATH 1
#define MAGIC_PROTOCOL 0x8311AA

#pragma pack(push, 1)
class LinkProperties {
public:
	// transfer parameters
	float RTT;			// propagation RTT (in sec)
	float speed;		// bottleneck bandwidth (in bits/sec)
	float pLoss[2];		// probability of loss in each direction
	DWORD bufferSize;	// buffer size of emulated routers (in packets)
	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class Flags {
public:
	DWORD reserved : 5;	// must be zero
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};
class SenderDataHeader {
public:
	Flags flags;
	DWORD seq;			// must begin from 0
};
class SenderSynHeader {
public:
	SenderDataHeader sdh;
	LinkProperties lp;
};
class ReceiverHeader {
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in pkts)
	DWORD ackSeq; // ack value = next expected sequence
};
class SenderDataPacket {
public:
	SenderDataHeader sdh;
	char data[MAX_PKT_SIZE];
};
#pragma pop

using namespace std;

class SenderSocket {

private:

	int send_syn_pckt();
	int recv_syn_pckt(long to_sec, long to_usec);
	int send_fin_pckt();
	int recv_fin_pckt(long to_sec, long to_usec);
	int send_data_pckt(char* buf, unsigned int buf_size);
	int recv_data_pckt(long to_sec, long to_usec);
	void calculate_est_rtt(float latest_sample);
	void calculate_dev_rtt(float latest_sample);
	void calculate_rto(float latest_sample);

public:

	string host;
	int port_no;
	int s_window;
	float rtt;
	float pckt_loss_f;
	float pckt_loss_r;
	float link_speed;
	UINT64 buff_size;
	int recv_window;

	int timeouts;
	int fast_retransmits;

	UINT64 bytes_acked;


	float est_rtt;
	float dev_rtt;

	unsigned int current_seq_no;

	float rto;

	bool open = false;

	clock_t begin_time;
	clock_t packet_timer;

	SOCKET sock;
	//int attempt_no = 0;
	struct sockaddr_in remote;
	struct sockaddr_in local;

	
	int Open(string host_n, int port_no_n, UINT64 buff_s_n, int senderWindow, float link_n, float rtt_n, float packet_loss_f, float packet_loss_r);
	int Send(char* buf, unsigned int buf_size);
	int Close();

	

};