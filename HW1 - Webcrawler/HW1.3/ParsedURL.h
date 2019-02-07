/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
ParsedURL.h
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

	ParsedURL(string url, bool verbose = false);

	string generateRequest(string requestType = "GET");
	string generateRobotRequest();
	char* getBaseUrl();
	bool isTamuURL();

	friend ostream & operator<<(ostream &out, ParsedURL &url);

};

