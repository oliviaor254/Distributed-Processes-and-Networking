/*modifURL.h: header file for modifURL class
*Connects a socket to the internet and recieves response.
*Created by: Olivia Ornelas
* Class: CSCE 463, section 500
* Semester: Spring 2021
* HW: Hw1 part 3
* Date: 02/13/2021
*/
#pragma once
#ifndef modifURL_h
#define modifURL_h
#define MAX_HOST_LEN		256
#define MAX_URL_LEN			2048
#include "pch.h"
#include "Parameters.h"
#include "HTMLParserBase.h"
class modifURL
{
private:
	string portNum;
	string path;
	string query;
	string fragment;
	string host;
	char* originalURL;
public:
	modifURL(char* baseURL);	//Parses URL
	void parse(LPVOID pParam, char* url, HTMLParserBase* parser);
	string GETRequestSyntax();
	string RobotRequestSyntax();
	string gethostName() { return host; };
	char* getBase();
	char* getURL() { return originalURL; };
	string getPort() { return portNum; };
	string getPath() { return path; }; //Used in previous hw1 parts
};
#endif
