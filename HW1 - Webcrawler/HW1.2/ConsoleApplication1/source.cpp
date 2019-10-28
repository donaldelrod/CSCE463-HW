/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
source.cpp
*/
#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <unordered_set>
#include <fstream>
#include "ParsedURL.h"
#include "Socket.h"
#include "HTMLParserBase.h"

using namespace std;

unordered_set<string> hosts;
unordered_set<string> ips;

//used to get multiple parameters back from sendToServer fuction
struct send_response {
	bool succ;
	IN_ADDR addr;

	send_response(bool s, IN_ADDR a) {
		succ = s;
		addr = a;
	}
};

/**
* Parses the header of the HTML response
*/
int parseHeader(string header, bool robots) {
	int statusCode = atoi(header.substr(9, 3).c_str());

	printf("\t  Verifying header... ");

	if (statusCode <= 99) {
		cout << "failed with non-HTTP header" << endl;
		return -1;
	}

	cout << "status code " << statusCode << endl;

	if (!robots && (statusCode < 200 || statusCode >= 300))
		return -1;
	else if (robots && statusCode < 400)
		return -1;

	return statusCode;
}

/**
* Parses the returned page for links
*/
void parsePage(ParsedURL url, char* page) {
	HTMLParserBase* parser = new HTMLParserBase;

	char* baseUrl = url.getBaseUrl();
	int nLinks;
	cout << "\t+ Parsing page...";
	double duration;
	clock_t timer = clock();
	char* linkBuffer = parser->Parse(page, (int)strlen(page), baseUrl, (int)strlen(baseUrl), &nLinks);
	duration = (clock() - timer) / (double)CLOCKS_PER_SEC;
	printf(" done in %.1f ms with %d links\n", duration * 1000, nLinks);
}

/**
* This function sends the request to the server and deals with the results
* returns true if successful, false otherwise
*/
send_response sendToServer(Socket s, ParsedURL url, bool robots) {

	if (robots) { //checks if the ip is unique, happens during robots stage
		IN_ADDR addr = s.presend(url);
		cout << "\t  checking ip uniqueness... ";
		int isize = ips.size();
		ips.insert(inet_ntoa(addr));
		if (isize != ips.size())
			cout << "passed" << endl;
		else {
			cout << "failed" << endl;
			return send_response(false, s.server.sin_addr);
		}
	}
	
	if (s.SendRequest(url, robots)) {//if the request is successfully sent
		
		if (s.Read(robots)) { //if the read was successful
			char* response = s.buf;

			closesocket(s.sock);

			string resStr = response;
			int endHeaderInd = resStr.find("\r\n\r\n");
			string header;

			if (endHeaderInd < 0)
				header = resStr;
			else
				header = resStr.substr(0, endHeaderInd);

			//trying to fix error at url http://abo.spiegel.de/de/c/spiegel-studenten-angebote/digital-testpaket
			//for some reason it returns a page of size 0 and this doesn't catch it for some reason
			if (strlen(header.c_str()) == 0) {
				cout << "HTML error, quitting..." << endl;
				return send_response(false, s.server.sin_addr);
			}

			int statusCode = parseHeader(header, robots);

			if (robots && statusCode >= 400) //should be successful for robots
				return send_response(true, s.server.sin_addr); //returning true because there is no robots.txt so unrestricted crawling
			else if (!robots && (statusCode < 300 && statusCode >= 200)) { //successful for a normal page

				if (strlen(resStr.c_str()) == 0) { //also trying to fix error when comminucating with http://abo.spiegel.de/de/c/spiegel-studenten-angebote/digital-testpaket
					cout << "HTML error, quitting..." << endl;
					return send_response(false, s.server.sin_addr); //request failed
				}
				parsePage(url, response);
			}
			else
				return send_response(false, s.server.sin_addr); //request failed
			
			return send_response(true, s.server.sin_addr); //request succeeded 
		}
		else return send_response(false, s.server.sin_addr); //request failed
	}
	else return send_response(false, s.server.sin_addr); //request failed
}
/**
* This function is the entry point of each overall web request
*/
bool queryURL(ParsedURL url, bool robots = false) {
	
	printf("URL: %s\n", url.original.c_str());
	cout << "\t  Parsing URL... ";
	if (url.validHost)
		cout << url.host << ", port " << url.port;
	else {
		cout << "failed with " << url.invalidReason << endl;
		return false;
	}

	if (!robots)
		cout << ", request " << url.path << endl;
	else cout << endl;

	cout << "\t  Checking host uniqueness... ";

	int psize = hosts.size();
	hosts.insert(url.host);
	if (psize == hosts.size()) {
		cout << "failed" << endl;
		return false;
	}
	else
		cout << "passed" << endl;

	Socket s = Socket();
	
	send_response sr1 = sendToServer(s, url, robots);

	bool success = sr1.succ;
	if (robots && sr1.succ) { //if robots are involved and the robot test passed
		s = Socket();
		s.server.sin_addr = sr1.addr; //copies IP address from previous socket to avoid DNS lookup again
		s.dns = true;
		send_response sr2 = sendToServer(s, url, false);
		success = sr2.succ;
	}
	return success;
}

int main(int argc, char** argv) {

	if (argc == 2) {
		string inputURL = argv[1];
		ParsedURL url(inputURL, false);
		queryURL(url);
	}
	else if (argc == 3) {
		int numThreads = atoi(argv[1]);

		if (numThreads != 1) {
			cout << "Can only run with 1 thread currently" << endl;
			return 0;
		}
		string inputFile = argv[2];
		vector<ParsedURL> urls;
		ifstream input(inputFile);
		string fileLine;

		//this filesize bit adapted from stackoverflow
		//https://stackoverflow.com/questions/5840148/how-can-i-get-a-files-size-in-c

		struct stat sb;
		int rc = stat(inputFile.c_str(), &sb);
		cout << "Input file: " << inputFile << "\t file size: " << sb.st_size << endl;

		int linksInMemory = 0;
		int lastFor = 0;

		//goes through the input file 100 lines at a time and fires off requests
		while (getline(input, fileLine)) {
			ParsedURL tURL(fileLine);
			urls.push_back(tURL);
			if (linksInMemory++ % 100 == 0) { //every 100 links, it will deplete the vector and empty it
				for (int i = 0; i < urls.size(); i++)
					queryURL(urls.at(i), true);
				urls = vector<ParsedURL>();
				linksInMemory = 0;
			}
		}
		for (int i = 0; i < urls.size(); i++) //finishes out the vector
			queryURL(urls.at(i), true);

	}
	else {
		cout << "Need URL input or number of threads and URL!" << endl;
		return 0;
	}	
	WSACleanup();
}