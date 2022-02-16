/*modifURL.cpp: .cpp file for modifURL class
* Prepares URL to be parsed, opens socket, performs p3 parsing
* Created by: Olivia Ornelas
* Class: CSCE 463, section 500
* Semester: Spring 2021
* HW: Hw1 part 3
* Date: 02/13/2021
*/
#include "pch.h"
#include "modifURL.h"
#include "Parameters.h"
#include "SocketM.h"
#include "HTMLParserBase.h"
modifURL::modifURL(char* baseURL) {
	originalURL = baseURL;
	string uRl;
	char temp[8];
	strncpy_s(temp, baseURL, 7);
	temp[sizeof(temp) - 1] = '\0';
	string HTTPsyntax(temp);
	string HT(baseURL);
	if (strncmp(HTTPsyntax.c_str(), "http://", 7) != 0) {//Checks if correct format of URL
		return;
	}
	size_t m = strlen(baseURL);
	size_t l = HTTPsyntax.size() + 1;
	uRl = HT.substr(HTTPsyntax.size());
	int fragp = uRl.find('#');
	if (fragp != string::npos) {
		fragment = uRl.substr(fragp);
		uRl = uRl.substr(0, fragp);
	}
	else { fragment = ""; }
	//Query
	int qP = uRl.find('?');
	if (qP != string::npos) {
		query = uRl.substr(qP); //Removes '?', obtains query
		uRl = uRl.substr(0, qP);
	}
	else { query = ""; }
	//Path value
	int pathP = uRl.find('/');
	if (pathP != string::npos) {
		if (uRl.at(pathP - 1) == ':') {
			return;
		}
		path = uRl.substr(pathP); //Removes '/', obtains path
		uRl = uRl.substr(0, pathP);
	}
	else {
		path = '/';
	}
	//Port #
	int portP = uRl.find(':');
	if (portP != string::npos) {
		portP = portP + 1;
		portNum = uRl.substr(portP); //Removes ':', obtains port
		if ((portNum.at(0) == 0) || (!isdigit(portNum.at(1)))) { //Accounts for if letter instead of a number
			printf("Invalid port value %s\n", portNum.c_str());
			return;
		}
		portP -= 1;
		host = uRl.substr(0, portP);
	}
	else {
		portNum = "80";
		host = uRl;//host is what should be left over
	}
}

char* modifURL::getBase() {
	string format = "http://" + host;
	char* base = new char[format.length() + 1];
	size_t length = format.length() + 1;
	strncpy_s(base, length, format.c_str(), length);
	return base;
}

string modifURL::GETRequestSyntax() {
	string request;
	request = "GET " + path + " HTTP/1.0\r\n";
	request += "User-agent: MyTAMUCrawler/1.1\r\n";
	request += "Host: " + host + "\r\n";
	request += "Connection: close\r\n";
	request += "\r\n";
	return request;
}

string modifURL::RobotRequestSyntax() {
	string requestT;
	requestT = "HEAD /robots.txt HTTP/1.0\r\n"; // Filename is Known (won't change)
	requestT += "User-agent: MyTAMUCrawler/1.1\r\n";
	requestT += "Host: " + host + "\r\n";
	requestT += "Connection: close\r\n";
	requestT += "\r\n";
	return requestT;
}
//Carries out p3 parsing
void modifURL::parse(LPVOID pParam, char* url, HTMLParserBase* pars) {
	SocketM socket1 = SocketM();
	string robotSend = RobotRequestSyntax();
	string GetSend = GETRequestSyntax();
	if (socket1.preSendChecks(host, portNum, pParam)) {
		if (socket1.LoadPage(robotSend, true, pParam, getBase(),pars)) { //sends on robots
			socket1.LoadPage(GetSend, false, pParam, getBase(),pars); //sends on robots
		}
	}
}