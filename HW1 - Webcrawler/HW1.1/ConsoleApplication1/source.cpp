/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
*/
#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <string.h>
#include <stdio.h>
#include "ParsedURL.h"
#include "Socket.h"
#include "HTMLParserBase.h"

using namespace std;

int main(int argc, char** argv) {

	if (argc != 2) {
		cout << "Need URL input!" << endl;
		return 0;
	}
	string inputURL = argv[1];

	ParsedURL url = ParsedURL(inputURL, false);
	
	//cout << "final parsed URL from ostream override is: " << endl << url << endl << endl;
	//cout << "request to be sent to server: " << endl << url.generateRequest() << endl;
	
	Socket s = Socket();
	printf("URL: %s\n", url.original.c_str());
	cout << "\t  Parsing URL... ";
	if (url.validHost)
		cout << url.host << ", port " << url.port << ", request " << url.path << endl;
	else {
		cout << "failed with " << url.invalidReason << endl;
		return 0;
	}

	if (s.SendRequest(url)) {
		if (s.Read()) {
			char* response = s.buf;
			closesocket(s.sock);
			
			string resStr = response;
			int endHeaderInd = resStr.find("\r\n\r\n");
			string header = resStr.substr(0, endHeaderInd);

			int statusCode = atoi(header.substr(9, 3).c_str());

			printf("\t  Verifying header... ");

			if (statusCode <= 99) {
				cout << "failed with non-HTTP header";
				return 0;
			}

			cout << "status code " << statusCode << endl;
			
			if (statusCode < 200 || statusCode >= 300)
				return 0;

			HTMLParserBase* parser = new HTMLParserBase;
		
			char* baseUrl = url.getBaseUrl();
			int nLinks;
			cout << "\t+ Parsing page...";
			clock_t timer;
			double duration;
			timer = clock();
			char* linkBuffer = parser->Parse(response, (int)strlen(response), baseUrl, (int)strlen(baseUrl), &nLinks);
			duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
			printf("done in %.1f ms with %d links\n", duration*1000, nLinks);

			cout << endl << "----------------------------------------"				<< endl;
			cout << header << endl;
		}
	}
	WSACleanup();
}