/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
*/
#pragma once
#include <string.h>
#include <iostream>
#include <sstream>

using namespace std;

class ParsedURL
{
public:
	string original;
	string protocol;
	string host;
	string path;
	string query;
	string fragment;
	string ip;
	int port;
	bool validHost;

	string invalidReason;

	
	ParsedURL();
	~ParsedURL();

	ParsedURL(string url, bool verbose);

	string generateRequest(string requestType = "GET");
	char* getBaseUrl();

	friend ostream & operator<<(ostream &out, ParsedURL &url);

};

