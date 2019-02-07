#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "pch.h"
#include <WinSock2.h>
#include <ws2tcpip.h>
#include "HTMLParserBase.h"
#include "ParsedURL.h"
#include <unordered_set>
#include <queue>
#include "Socket.h"
#include <iostream>
#include <stdio.h>
#include <fstream>

using namespace std;

//static int dns_successes;
//static int extracted_urls;
//static int crawled_urls;
//static int total_links;
//static int robot_successes;
//static int r2xx, r3xx, r4xx, r5xx;
//static long downloaded_total;

struct Parameters {
	HANDLE urlListMutex;
	HANDLE urlListEmptySemaphore, urlListFullSemaphore;
	HANDLE ipListMutex;
	HANDLE hostListMutex;
	//HANDLE finished;
	HANDLE eventQuit;
	HANDLE staticMutex;
	unordered_set<string> host_set;
	unordered_set<string> ip_set;
	queue<ParsedURL> urls;
	string fileName;
	bool done;

	volatile int dns_successes;
	volatile int extracted_urls;
	volatile int crawled_urls;
	volatile int total_links;
	volatile int robot_successes;
	volatile int r2xx, r3xx, r4xx, r5xx, rother;
	UINT64 downloaded_total;
	long total_time;
	int tamu_internal;
	int tamu_external;
	int running_threads;
};

/**
* Needs to be passed the server object to add DNS information into it
* Return value of -1 means invalid ip
* return value of 0 means operation successful
*/
int DNSLookup(string host, sockaddr_in &server) {
	
	hostent *remote;

	DWORD IP = inet_addr(host.c_str());
	if (IP == INADDR_NONE) {
		if ((remote = gethostbyname(host.c_str())) == NULL) {

			//cout << " invalid host: not FQDM or IP address" << endl;
			return -1;
		}
		else {
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
			return 0;
		}
	}

	server.sin_addr.S_un.S_addr = IP;

	return 0;
}

/**
* Parses the returned page for links
*/
int parsePage(ParsedURL url, char* page, HTMLParserBase* parser, Parameters* p) {

	char* baseUrl = url.getBaseUrl();
	int nLinks;
	//cout << "\t+ Parsing page...";
	//double duration;
	//clock_t timer = clock();
	char* linkBuffer = parser->Parse(page, (int)strlen(page), baseUrl, (int)strlen(baseUrl), &nLinks);

	int tamuLinks = 0;
	string tLink;

	for (int i = 0; i < nLinks; i++) {
		tLink = string(linkBuffer);
		ParsedURL turl(tLink);
		if (turl.isTamuURL())
			tamuLinks++;
		//printf("%s\n", linkBuffer);
		linkBuffer += strlen(linkBuffer) + 1;
	}
	if (tamuLinks !=0) {
		WaitForSingleObject(p->staticMutex, INFINITE);
		if (url.isTamuURL())
			p->tamu_internal += tamuLinks;
		else
			p->tamu_external += tamuLinks;
		ReleaseMutex(p->staticMutex);
	}

	//duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
	//printf(" done in %.1f ms with %d links\n", duration * 1000, nLinks);
	delete baseUrl;
	return nLinks;
	//totalLinks += nLinks;
}

/**
* Parses the header of the HTML response
*/
int parseHeader(string header) {
	//printf("\t  Verifying header... ");
	int statusCode = -1;

	if (header.length() > 14)
		statusCode = atoi(header.substr(9, 3).c_str());

	if (statusCode <= 99 || statusCode > 505) {
		//cout << "failed with non-HTTP header" << endl;
		return -1;
	}

	return statusCode;
}

UINT crawlThread(LPVOID crawlParam) {

	//Sleep(5000);

	HTMLParserBase* parser = new HTMLParserBase;
	Parameters *p = (Parameters*)crawlParam;
	unordered_set<string>* ip_set = &p->ip_set;
	unordered_set<string>* host_set = &p->host_set;
	queue<ParsedURL> *url_list = &p->urls;
	HANDLE hostListMutex = p->hostListMutex;
	HANDLE ipListMutex = p->ipListMutex;
	HANDLE urlQueueMutex = p->urlListMutex;
	HANDLE staticMutex = p->staticMutex;
	HANDLE urlQueueFullSemaphore = p->urlListFullSemaphore;
	HANDLE urlQueueEmptySemaphore = p->urlListEmptySemaphore;
	Socket s = Socket();

	ParsedURL currentURL;
	ParsedURL tamuURL = ParsedURL("http://tamu.edu/");

	//printf("crawl thread started: %d\n", GetCurrentThreadId());

	while (1) {

		//Sleep(200);
		
		s.Reinit();

		bool host_unique = false;
		//printf("wfso uqfs\n");
		WaitForSingleObject(urlQueueFullSemaphore, INFINITE);
		//printf("wfso uqm\n");
		//start queue critical section
		WaitForSingleObject(urlQueueMutex, INFINITE);

		if (url_list->size() == 0) {
			//printf("url list empty\n");
			ReleaseMutex(urlQueueMutex);
			ReleaseSemaphore(urlQueueFullSemaphore, 2, NULL);
			WaitForSingleObject(staticMutex, INFINITE);
			p->running_threads--;
			ReleaseMutex(staticMutex);
			//ReleaseSemaphore(p->finished, 1, NULL);
			//WaitForSingleObject(p->eventQuit, INFINITE);
			//printf("returning from crawlThread...\n");
			return 0;
		}
		

		try {

			currentURL = url_list->front();
			url_list->pop();

		}
		catch (int errno) {
			ReleaseMutex(urlQueueMutex);
			//end queue critical section
			ReleaseSemaphore(urlQueueFullSemaphore, 1, NULL);
			//Sleep(500);
			continue;
		}

		ReleaseMutex(urlQueueMutex);
		//end queue critical section
		ReleaseSemaphore(urlQueueEmptySemaphore, 1, NULL);

		//bool isTamuUrl = currentURL.isTamuURL();

		//InterlockedIncrement((long*)&(p->extracted_urls));
		WaitForSingleObject(staticMutex, INFINITE);
		p->extracted_urls++;
		//if (isTamuUrl)
		//	p->tamu_hosts++;
		ReleaseMutex(staticMutex);

		//start host critical section
		WaitForSingleObject(hostListMutex, INFINITE);


		int hsize = host_set->size();
		host_set->insert(currentURL.host);
		if (hsize != host_set->size())
			host_unique = true;

		ReleaseMutex(hostListMutex);
		//end host critical section
		

		if (!host_unique) //if host is not unique, continue
			continue;

		sockaddr_in server;
		int dns_result = DNSLookup(currentURL.host, server);

		if (dns_result == -1) { // if the dns was not successful
			//ReleaseMutex(ipListMutex);
			//end ip critical section early if the dns result is bad
			//it's in this critical area so as to avoid concurrency issues
			//with the static variable dns_successes
			continue;
		}

		//InterlockedIncrement((long*)&(p->dns_successes));
		WaitForSingleObject(staticMutex, INFINITE);
		p->dns_successes++;
		ReleaseMutex(staticMutex);

		//start ip critical section
		WaitForSingleObject(ipListMutex, INFINITE);
		int isize = ip_set->size();
		ip_set->insert(inet_ntoa(server.sin_addr));
		if (isize == ip_set->size()) { //failed
			ReleaseMutex(ipListMutex);
			continue;
		}

		ReleaseMutex(ipListMutex);
		//end ip critical section

		//at this point it has passed host and ip uniqueness and dns was successful

		string robotRequest = currentURL.generateRobotRequest(),
			pageRequest = currentURL.generateRequest();

		if (s.SendRequest(currentURL, server, robotRequest)) {
			if (s.Read()) {
				char* response = s.buf;

				//InterlockedAdd(&(p->downloaded_total), strlen(response));
				WaitForSingleObject(staticMutex, INFINITE);
				p->downloaded_total += strlen(response);
				ReleaseMutex(staticMutex);


				closesocket(s.sock);

				string responseStr = response;
				int endHeader = responseStr.find("\r\n\r\n");
				string header;

				if (endHeader < 0)
					header = responseStr;
				else
					header = responseStr.substr(0, endHeader);

				int statusCode = parseHeader(header);

				if (statusCode == -1) {
					continue;
				}
				WaitForSingleObject(staticMutex, INFINITE);
				if (statusCode >= 200 && statusCode < 300)
					p->r2xx++;
				else if (statusCode >= 300 && statusCode < 400)
					p->r3xx++;
				else if (statusCode >= 400 && statusCode < 500)
					p->r4xx++;
				else if (statusCode >= 500 && statusCode < 600)
					p->r5xx++;
				else p->rother++;


				if (statusCode >= 400 && statusCode < 500) {
					s.Reinit();

					WaitForSingleObject(staticMutex, INFINITE);
					p->robot_successes++;
					ReleaseMutex(staticMutex);

					//now ready to send real request
					if (s.SendRequest(currentURL, server, pageRequest)) {
						if (s.Read()) {
							response = s.buf;
							//InterlockedAdd(&(p->downloaded_total), strlen(response));
							WaitForSingleObject(staticMutex, INFINITE);
							p->downloaded_total += strlen(response);
							ReleaseMutex(staticMutex);

							closesocket(s.sock);

							responseStr = response;
							endHeader = responseStr.find("\r\n\r\n");
							header;

							if (endHeader < 0)
								header = responseStr;
							else
								header = responseStr.substr(0, endHeader);

							int statusCode = parseHeader(header);

							if (statusCode >= 200 && statusCode < 300) {
								
								int numLinks = parsePage(currentURL, response, parser, p);
								
								
								
								bool internalTamu = currentURL.isTamuURL();
								
								//increase static variable
								WaitForSingleObject(staticMutex, INFINITE);
								p->total_links += numLinks;
								//InterlockedAdd((long*)&(p->total_links), numLinks);
								p->crawled_urls++;
								//InterlockedIncrement((long*)&(p->crawled_urls));
								ReleaseMutex(staticMutex);
							}
						}
					}
				}
				else continue;
			}
		}
	}
}

UINT statThread(LPVOID statParams) {

	//printf("stat thread started: %d\n", GetCurrentThreadId());

	Parameters *p = (Parameters*)statParams;
	clock_t start = clock();
	long download_diff = 0;
	int pages_diff = 0;
	bool lastLoop = false;
	while (1) {
		download_diff = p->downloaded_total;
		pages_diff = p->crawled_urls;
		Sleep(2000);

		string link_string = to_string(p->total_links);

		if (p->total_links >= 1000 && p->total_links < 1000000)
			link_string = to_string(p->total_links / 1000) + "k";
		else if (p->total_links >= 1000000 && p->total_links < 1000000000)
			link_string = to_string(p->total_links / 1000000) + "M";
		else if (p->total_links >= 1000000000 && p->total_links < 1000000000000)
			link_string = to_string(p->total_links / 1000000000) + "B";
		else if (p->total_links >= 1000000000000 && p->total_links < 1000000000000000)
			link_string = to_string(p->total_links / 1000000000000) + "T";
		
		printf("[%3d] %4d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %s\n",
			(int)floor((clock() - start) / (double)CLOCKS_PER_SEC), //elapsed time in seconds
			(int)p->running_threads,// number of currently running threads
			(int)p->urls.size(),	//Q number of urls in queue
			(int)p->extracted_urls, //E number of urls taken out of the queue
			(int)p->host_set.size(),//H number of unique hosts
			(int)p->dns_successes,	//D number of successfule dns lookups
			(int)p->ip_set.size(),	//I number of unique ip addresses
			(int)p->robot_successes,//R number of successful robot pages
			(int)p->crawled_urls,	//C the number of urls crawled so far
			link_string				//L the total number of links found so far
		);

		printf("\t*** crawling %.1f pps @ %.1f Mbps\n",
			(double)(p->crawled_urls - pages_diff)/(double)2, 
			(double)(p->downloaded_total- download_diff)/(double)2000000 
		);
		if (p->running_threads == 0 && !lastLoop) {
			lastLoop = true;
		}
		else if (p->running_threads == 0 && lastLoop) {
			break;
		}
			
	}
	p->total_time = (clock() - start) / (double)CLOCKS_PER_SEC;
	//ReleaseSemaphore(p->urlListFullSemaphore, 10, NULL);
	//printf("stat thread exiting... time was %li\n", p->total_time);
	return 0;
}

UINT fileThread(LPVOID fileParams) {

	//printf("file thread started: %d\n", GetCurrentThreadId());

	Parameters *p = (Parameters*)fileParams;
	string fileLine;
	ifstream input(p->fileName);
	HANDLE urlQueueMutex = p->urlListMutex;
	HANDLE urlQueueFullSemaphore = p->urlListFullSemaphore;
	HANDLE urlQueueEmptySemaphore = p->urlListEmptySemaphore;

	//this filesize bit adapted from stackoverflow
	//https://stackoverflow.com/questions/5840148/how-can-i-get-a-files-size-in-c

	struct stat sb;
	int rc = stat(p->fileName.c_str(), &sb);
	cout << "Input file: " << p->fileName << "\t file size: " << sb.st_size << endl;

	while (getline(input, fileLine)) {
		WaitForSingleObject(urlQueueEmptySemaphore, INFINITE);
		//WaitForSingleObject(urlQueueMutex, INFINITE);
		p->urls.emplace(ParsedURL(fileLine));
		//ReleaseMutex(urlQueueMutex);
		ReleaseSemaphore(urlQueueFullSemaphore, 1, NULL);
	}
	p->done = true;
	return 0;
}


int main(int argc, char** argv) {

	if (argc == 3) {
		int numThreads = atoi(argv[1]);

		string inputFile = argv[2];
		vector<ParsedURL> urls;
		int listSize = 1000010;
		Parameters params;
		params.ipListMutex = CreateMutex(NULL, 0, NULL);
		params.urlListMutex = CreateMutex(NULL, 0, NULL);
		params.urlListEmptySemaphore = CreateSemaphore(NULL, listSize, listSize, NULL);
		params.urlListFullSemaphore = CreateSemaphore(NULL, 1, listSize, NULL);
		params.hostListMutex = CreateMutex(NULL, 0, NULL);
		//params.staticMutex = CreateMutex(NULL, 0, NULL);
		params.ip_set = unordered_set<string>();
		params.host_set = unordered_set<string>();
		params.urls = queue<ParsedURL>();
		params.fileName = inputFile;
		//params.finished = CreateSemaphore(NULL, 0, numThreads, NULL);
		params.eventQuit = CreateEvent(NULL, true, false, NULL);
		params.dns_successes = 0;
		params.extracted_urls = 0;
		params.crawled_urls = 0;
		params.total_links = 0;
		params.robot_successes = 0;
		params.r2xx = 0;
		params.r3xx = 0;
		params.r4xx = 0;
		params.r5xx = 0;
		params.rother = 0;
		params.downloaded_total = 0;
		params.done = false;
		params.total_time = 0;
		params.tamu_internal = 0;
		params.tamu_external = 0;
		params.running_threads = numThreads;
		
		


		HANDLE fileHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fileThread, &params, 0, NULL);

		WaitForSingleObject(fileHandle, INFINITE);
		CloseHandle(fileHandle);
		

		HANDLE *threadHandles = new HANDLE[numThreads];
		for (int i = 0; i < numThreads; i++) {
			threadHandles[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawlThread, &params, 0, NULL);
		}

		HANDLE statHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statThread, &params, 0, NULL);

		

		for (int i = 0; i < numThreads; i++) {
			WaitForSingleObject(threadHandles[i], INFINITE);
			CloseHandle(threadHandles[i]);
		}

		WaitForSingleObject(statHandle, INFINITE);
		CloseHandle(statHandle);

		printf("\nExtracted %d URLs @ %d/s\n", params.extracted_urls, params.extracted_urls / params.total_time);
		printf("Looked up %d DNS names @ %d/s\n", params.dns_successes, params.dns_successes / params.total_time);
		printf("Downloaded %d robots @ %d/s\n", params.robot_successes, params.robot_successes / params.total_time);
		printf("Crawled %d pages @ %d/s (%.2f MB)\n", params.crawled_urls, params.crawled_urls / params.total_time, (double)(params.downloaded_total / (double)1048576));
		printf("Parsed %d links @ %d/s\n", params.total_links, params.total_links / params.total_time);
		printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n", params.r2xx, params.r3xx, params.r4xx, params.r5xx, params.rother);
		printf("Number of links that point to \"*tamu.edu/\": %d\n", params.tamu_internal + params.tamu_external);
		printf("Number of external links that point to \"*tamu.edu/\": %d\n", params.tamu_external);

	}
	else {
		cout << "Need number of threads and filename!" << endl;
		return 0;
	}
	WSACleanup();
}