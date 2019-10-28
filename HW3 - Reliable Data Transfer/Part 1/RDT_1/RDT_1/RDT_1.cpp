// RDT_1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <string>
#include <string.h>
#include "SenderSocket.h"

using namespace std;

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

	
	printf("Main:\tsender W = %d, RTT = %g sec, loss %g / %g, link %d\n",
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

	printf("Main: connected to %s in %.3f sec, packet size %d bytes\n", host.c_str(), ss.rto / 3, ss.recv_window + 3);

	if ((status = ss.Close()) != STATUS_OK) {
		printf("disconnect failed with status %d\n", status);
		return 0;
	}

	printf("Main:\ttransfer finished in %.3f sec\n", (clock() - ss.packet_timer) / (double)CLOCKS_PER_SEC);
}