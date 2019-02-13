/**
Donald Elrod
824006067
CSCE 463 - Spring 2019
ParsedURL.cpp
*/
#pragma once
#include "pch.h"
#include "ParsedURL.h"

//Generic, unused constructor for the ParsedURL object
ParsedURL::ParsedURL()
{}

/**
* This constructor should be used when constructing a ParsedURL object, as it splits the URL into parts
* @param {string} url: the string containing the website to crawl, e.g. http://www.google.com/account?username=donaldelrod#contactInformation
* @param {bool} verbose: optional... whether you want verbose output or not, false by default
* @returns {ParsedURL} returns a well formed ParsedURL object
*/
ParsedURL::ParsedURL(string url, bool verbose) {
	original = url;
	int fragPos, queryPos, pathPos, portPos;

	if (strcmp(url.substr(0, 7).c_str(), "http://") == 0) {
		url = url.substr(7);
		protocol = "http";
	}
	else if (strcmp(url.substr(0, 8).c_str(), "https://") == 0) {
		url = url.substr(8);
		protocol = "https";
	}
	else protocol = "http"; //default if protocol is somehow omitted

	if (verbose) cout << "passed url: " << url.c_str() << endl;

	if ((fragPos = (int)url.find("#")) > 0) {
		fragment = url.substr(fragPos + 1);
		url = url.substr(0, fragPos);
		if (verbose) cout << "url after truncation: " << url.c_str() << "\t" << "fragment grabbed: " << fragment.c_str() << endl;
	}
	else
		fragment = "";

	if ((queryPos = (int)url.find("?")) > 0) {
		query = url.substr(queryPos + 1);
		url = url.substr(0, queryPos);
		if (verbose) cout << "url after truncation: " << url.c_str() << "\t" << "query grabbed: " << query.c_str() << endl;
	}
	else
		query = "";

	if ((pathPos = (int)url.find("/")) > 0) {
		path = url.substr(pathPos);
		url = url.substr(0, pathPos);
		if (verbose) cout << "url after truncation: " << url.c_str() << "\t" << "path grabbed: " << path.c_str() << endl;
	}
	else {
		path = "/";
		if (verbose) cout << "url not truncated as no path found: " << url.c_str() << "\t" << "path grabbed: " << path.c_str() << endl;
	}

	if ((portPos = (int)url.find(":")) > 0) {
		string portStr = url.substr(portPos + 1);
		url = url.substr(0, portPos);

		port = atoi(portStr.c_str());
		if (port <= 0) {
			invalidReason = "failed with invalid port";
			validHost = false;
			return;
		}

		if (verbose) cout << "url after truncation: " << url.c_str() << "\t" << "port grabbed: " << portStr.c_str() << endl;
	}
	else {
		port = 80;
		if (verbose) cout << "port is defaulting to port 80 as none was specified" << endl;
	}
	validHost = true;
	host = url;
}

/**
* Generates the HTTP 1.0 request for this object
* @param {string} requestType: optional... default is GET
* @returns {string} returns a well formed request to the resource contained in this ParsedURL object
*/
string ParsedURL::generateRequest(string requestType) {
	ostringstream oss;
	oss << requestType << " " << path;
	if (strcmp(query.c_str(), "") != 0)
		oss << "?" << query.c_str();
	oss << " HTTP/1.0\r\n";
	oss << "User-agent: TAMUCrawler/1.1\r\n";
	oss << "Host: " << host << "\r\n";
	oss << "Connection: close\r\n\r\n";
	return oss.str();
}

/**
* Generates the HTTP 1.0 HEAD request for this object's host robots.txt file
* @returns {string} returns a well formed HEAD request to the robots.txt of the resource contained in this ParsedURL object
*/
string ParsedURL::generateRobotRequest() {
	ostringstream oss;
	oss << "HEAD /robots.txt";
	oss << " HTTP/1.0\r\n";
	oss << "User-agent: TAMUCrawler/1.1\r\n";
	oss << "Host: " << host << "\r\n";
	oss << "Connection: close\r\n\r\n";
	return oss.str();
}

/**
* Gets the base URL of this ParsedURL object (just the protocol and host, no path query or fragment
* @returns {char*} returns the base URL as a char*
*/
char * ParsedURL::getBaseUrl()
{
	ostringstream oss;
	oss << protocol << "://" << host;
	string formatted = oss.str();
	char* baseUrl = new char[formatted.length() + 1];
	strcpy_s(baseUrl, (int)strlen(formatted.c_str()) + 1, formatted.c_str());
	return baseUrl;
}

/**
* Checks if this ParsedURL object has a URL hosted from TAMU's servers
* @returns {bool} returns true if it is a TAMU URL, false if not
*/
bool ParsedURL::isTamuURL() {
	if (host.find("tamu.edu") != -1 && original.find("tamu.edu/") != -1)
		return true;
	else return false;
}

//Doesn't work, idk not really needed I think
ParsedURL::~ParsedURL() {
	/*delete &original;
	delete &host;
	delete &path;
	delete &port;
	delete &query;
	delete &fragment;
	delete &protocol;
	delete &ip;
	delete &invalidReason;
	delete &dns_res;
	delete &dns;
	delete &validHost;*/
}

/**
* Override for the ostream operator, prints the entire url to the whatever ostream you are using
*/
ostream & operator<<(ostream &out, ParsedURL &url)
{
	out << url.protocol.c_str() << "://" << url.host.c_str() << ":" << url.port << url.path.c_str();
	if (strcmp(url.query.c_str(), "") != 0)
		out << "?" << url.query.c_str();
	if (strcmp(url.fragment.c_str(), "") != 0)
		out << "#" << url.fragment.c_str();
	out << endl << endl << url.generateRequest() << endl << url.generateRobotRequest();

	return out;
}