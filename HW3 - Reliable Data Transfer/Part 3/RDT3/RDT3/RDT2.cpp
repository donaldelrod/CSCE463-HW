// RDT_1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <string>
#include <string.h>
#include "SenderSocket.h"
#include "Checksum.h"

using namespace std;

struct params {
	SenderSocket* ss;
	bool stop;
	float av_rate;
};

UINT stat_thread(LPVOID stat_params) {
	
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	
	params *p = (params*)stat_params;
	SenderSocket* ss = (SenderSocket*)p->ss;

	clock_t start = clock();

	int last_base_no, base_diff;

	float av_rate = 0;
	int num_points = 0;

	while (!p->stop) {
		//UINT64 bytes_before = ss->bytes_acked;
		last_base_no = ss->current_seq_no;

		Sleep(2000);
		//double elapsed = floor((clock() - start) / (double)CLOCKS_PER_SEC);

		double mb_acked = ss->bytes_acked / 1e6;
		
		double elapsed = (clock() - ss->begin_time) / (double)CLOCKS_PER_SEC;

		base_diff = ss->current_seq_no - last_base_no;
		//UINT64 diff = ss->bytes_acked - bytes_before;

		double speed = (base_diff * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)))/1e7;
		double tot = av_rate * num_points;
		num_points++;

		av_rate = (tot + speed) / (double)num_points;

		printf("[%3.0f] B %5d ( %.1f MB ) N %5d T %2d F 0 W 1 S %0.3f Mbps RTT %.3f\n",
			floor(elapsed), ss->current_seq_no, mb_acked, ss->current_seq_no+1,
			ss->timeouts, speed, ss->est_rtt);
	}

	p->av_rate = av_rate;

	return 0;
}

int main(int argc, char* argv[]) {
	if (argc != 8) {
		cout << "Inproper number of input args" << endl;
		return 1;
	}

	string host = argv[1];
	int power = atoi(argv[2]);
	int window = atoi(argv[3]);
	float rtt = atof(argv[4]);
	float packet_loss_forward = atof(argv[5]);
	float packet_loss_reverse = atof(argv[6]);
	float link_speed = atof(argv[7]);


	printf("Main:\tsender W = %d, RTT = %g sec, loss %g / %g, link %.0f\n",
		window, rtt, packet_loss_forward, packet_loss_reverse, link_speed);
	printf("Main:\tinitializing DWORD array with 2^%d elements...", power);

	clock_t timer = clock();
	double duration;

	//initialize buffer
	UINT64 dwordBufSize = (UINT64)1 << power;

	UINT64 packet_size = ceil(dwordBufSize / window);

	DWORD *dwordBuf = new DWORD[dwordBufSize]; // user-requested buffer
	for (UINT64 i = 0; i < dwordBufSize; i++) // required initialization
		dwordBuf[i] = i;

	duration = (double)(clock() - timer) / (double)CLOCKS_PER_SEC;
	printf(" done in %d ms\n", duration * 1000);

	SenderSocket ss;
	int status;
	if ((status = ss.Open(host, MAGIC_PORT, dwordBufSize, window, link_speed, rtt, packet_loss_forward, packet_loss_reverse)) != STATUS_OK) {
		printf("connect failed with status %d\n", status);
		return 0;
	}

	printf("Main:\tconnected to %s in %.5f sec, pkt %d bytes\n", host.c_str(), ss.rto / 3, MAX_PKT_SIZE);

	char *charBuf = (char*)dwordBuf; // this buffer goes into socket
	UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes

	struct params stat_params;
	stat_params.ss = &ss;
	stat_params.stop = false;

	HANDLE stat_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)stat_thread, &stat_params, 0, NULL);

	UINT64 off = 0; // current position in buffer
	while (off < byteBufferSize) {
		// decide the size of next chunk
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		// send chunk into socket
		if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK) {
			printf("error in ss.Send: %d\n", status);
			// error handing: print status and quit
		}
		off += bytes;

		//Sleep(1000);
	}

	double finish_time = (clock() - timer) / (double)CLOCKS_PER_SEC;

	stat_params.stop = true;

	WaitForSingleObject(stat_handle, INFINITE);
	CloseHandle(stat_handle);

	if ((status = ss.Close()) != STATUS_OK) {
		printf("disconnect failed with status %d\n", status);
		return 0;
	}

	Checksum cs;

	printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %X\n", finish_time, stat_params.av_rate * 1e3, cs.CRC32((unsigned char*)charBuf, byteBufferSize));


	printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n", ss.est_rtt, 1/ss.est_rtt);
}