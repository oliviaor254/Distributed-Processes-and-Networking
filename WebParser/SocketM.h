/*SocketM.h: Header file to instantiate SocketM class.
*Connects a socket to the internet, recieves response, and checks/updates headers.
*Created by: Olivia Ornelas
* Class: CSCE 463, section 500
* Semester: Spring 2021
* HW: Hw1 part 3
* Date: 02/13/2021
*/
#pragma once
#ifndef SocketM_h
#define SocketM_h
#define INITIAL_BUF_SIZE    512
#define TIMEOUT_ASYNC       10000
#define THRESHOLD           6048
#define MAX_ROBOT_BUFFERSIZE 16384
#define MAX_PAGE_BUFFERSIZE  2097152
#include "pch.h"
#include "modifURL.h"
#include "Parameters.h"
#include "HTMLParserBase.h"
class SocketM {
private:
	SOCKET sock1; // socket handle
	char* buf; // current buffer
	timeval timeout;
	int allocatedSize; // bytes allocated for buf
	FD_SET fd;
	struct hostent* remote;
	// structure for connecting to server
	struct sockaddr_in server;

public:
	bool Read(bool robots, int MAXSIZE);
	char* getbuf();
	bool preSendChecks(string hostname, string port, LPVOID pParam);
	int getBufferSize();
	SocketM();
	bool LoadPage(string req, bool robotsPhase, LPVOID pParam, char* base, HTMLParserBase* parser);
	void updateAllStatus(int status, LPVOID pParam);
	void Count(char* base, LPVOID pParam, HTMLParserBase* parser);
};
#endif