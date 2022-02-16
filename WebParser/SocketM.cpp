/*SocketM.cpp: .cpp file for SocketM header file
*Connects a socket to the internet, recieves response, and checks/updates headers.
*Created by: Olivia Ornelas
* Class: CSCE 463, section 500
* Semester: Spring 2021
* HW: Hw1 part 3
* Date: 02/13/2021
*/
#include "pch.h"
#include "SocketM.h"
#include "Parameters.h"
#include "HTMLParserBase.h"
SocketM::SocketM()
{
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	buf = new char[INITIAL_BUF_SIZE];
	allocatedSize = INITIAL_BUF_SIZE;
	WSADATA wsaData;
	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		//printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	sock1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //Open socket
	if (sock1 == INVALID_SOCKET)
	{
		//printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	FD_ZERO(&fd);
	FD_SET(sock1, &fd);
}
bool SocketM::preSendChecks(string hostname, string port, LPVOID lpParam) {
	Parameters* p = (Parameters*)lpParam;
	//Checks host uniqueness
	WaitForSingleObject(p->ParamMutex, INFINITE);
	size_t prevHostSize = p->seenHosts.size();
	p->seenHosts.insert(hostname);
	if (prevHostSize == p->seenHosts.size()) {
		ReleaseMutex(p->ParamMutex);
		closesocket(sock1);
		return false;
	}
	p->numUniqueHosts++;
	ReleaseMutex(p->ParamMutex);
	//Performs DNS
	DWORD IP2 = inet_addr(hostname.c_str());
	 struct hostent* remote = gethostbyname(hostname.c_str());
	 if (IP2 == INADDR_NONE) {
		 if (remote != NULL) {
			 memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
			 InterlockedIncrement((LONG*)&p->SuccessfulDNS);
		 }
	 }
		else{ server.sin_addr.S_un.S_addr = IP2; }
	 if (server.sin_addr.S_un.S_addr == NULL) {
		 return false;
	 }
	 //Checking IP uniqueness
		WaitForSingleObject(p->ParamMutex, INFINITE);
		size_t prevSize = p->seenIps.size();
		p->seenIps.insert(server.sin_addr.S_un.S_addr);
		if (p->seenIps.size() == prevSize) {
			ReleaseMutex(p->ParamMutex);
			return false;
		}
		p->numUniqueIPs++;
		ReleaseMutex(p->ParamMutex);
		//Sets up server for connection
		server.sin_family = AF_INET;
		server.sin_port = htons(atoi(port.c_str()));

	return true;
}
int SocketM::getBufferSize() {
	return allocatedSize;
}
char* SocketM::getbuf() {
	return buf;
}

bool SocketM::LoadPage(string req, bool robotsPhase, LPVOID pParam, char* base, HTMLParserBase* parser) {
	Parameters* p = (Parameters*)pParam;
	int s, h, x;
	if (connect(sock1, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		//cout << "failed connect " << WSAGetLastError() << endl;
		return false;
	}
	if (send(sock1, req.c_str(), strlen(req.c_str()), 0) == SOCKET_ERROR) {
		//printf("Sending failed with %d\n", WSAGetLastError());
		closesocket(sock1);
		WSACleanup();
		return false;
	}

	if (robotsPhase) {
		//Make call to robots and read the buffer
		if (Read(true, MAX_ROBOT_BUFFERSIZE)) {
			WaitForSingleObject(p->ParamMutex, INFINITE);
			p->BytesDownloaded += strlen(buf);
			ReleaseMutex(p->ParamMutex);
			if ((strlen(buf) > 9) && (buf[9] == '4')) {
					InterlockedIncrement((LONG*)&p->numRobotPassed);
					return true;
			}
		}
		return false;
	}
	else if (Read(false, MAX_PAGE_BUFFERSIZE)) {
		//Get status
		x = sscanf_s(buf, "HTTP/1.%d %d", &h, &s);
		//Read page from buffer
		WaitForSingleObject(p->ParamMutex, INFINITE);
		p->BytesDownloaded += strlen(buf);
		ReleaseMutex(p->ParamMutex);
		if (strlen(buf) > 9) {
			updateAllStatus(s, pParam); //Updates status before anything else
			if (buf[9] == '2') {
				InterlockedIncrement((LONG*)&p->CrawledCount);
				Count(base,pParam,parser); //Parses links from page
				return true;
			}
		}
	}
	return false;
}
//Updates status counts
void SocketM::updateAllStatus(int status, LPVOID pParam) {
	Parameters* p = (Parameters*)pParam;
	WaitForSingleObject(p->ParamMutex, INFINITE);
	if ((status >= 500) && (status < 600)) {
		p->Status5xx++;
	}
	else if ((status >= 400) && (status < 500)) {
		p->Status4xx++;
	}
	else if ((status >= 300) && (status < 400)) {
		p->Status3xx++;
	}
	else if ((status >= 200) && (status < 300)) {
		p->Status2xx++;
	}
	else {
		p->OtherStatus++;
	}
	ReleaseMutex(p->ParamMutex);
}

bool SocketM::Read(bool robots, int MAXSIZE)
{
	int curPos = 0;
	int ret;
	int spaceleft;

	curPos = 0;
	while (true)
	{
		// wait to see if socket has any data (see MSDN)
		if ((ret = select(1, &fd, 0, 0, &timeout)) > 0)
		{
			// new data available; now read the next segment
			spaceleft = allocatedSize - curPos;
			int bytes = recv(sock1, buf + curPos, spaceleft, 0);
			if (bytes < 0) {
				//	printf("Socket() generated error %d on recv\n", WSAGetLastError());
				break;
			}
			if (bytes == 0)
			{ // NULL-terminate buffer
				closesocket(sock1);
				WSACleanup();
				return true; // normal completion (only time it should return true)
			}
			curPos += bytes; // adjust where the next recv goes, Changes value of spaceleft
			if ((allocatedSize - curPos) < INITIAL_BUF_SIZE) {
				// resize buffer
				if (allocatedSize >= MAXSIZE) {
					if (robots) {
						//			printf("Robots header too large, aborting...\n");
						break;
					}
					else {
						//		printf("Page too large, aborting...\n");
						break;
					}
				}
				char* tempBuf = new char[allocatedSize];
				memcpy(tempBuf, buf, allocatedSize);
				allocatedSize *= 2;
				buf = new char[allocatedSize];
				memcpy(buf, tempBuf, allocatedSize / 2); //Copying second buffer into original
				memset(&tempBuf[0], 0, sizeof(tempBuf)); //Clearing second buffer
			}

		}
		else if (ret == 0) {
			break;
		}
		else {
			break;
		}
	}
	closesocket(sock1);
	WSACleanup();
	return false;
}
void SocketM::Count(char* base, LPVOID pParam, HTMLParserBase* parser) {
	Parameters* p = (Parameters*)pParam;
	int nLinks = 0;
	int tamuCount = 0;
	string tamuCheck;
	int FromT = 0;
	char* linkBuf = parser->Parse(buf, strlen(buf), base, strlen(base), &nLinks);
	WaitForSingleObject(p->ParamMutex, INFINITE);
	p->numLinks += nLinks;
	ReleaseMutex(p->ParamMutex);
	//********Checking TAMU links
	string baseCheck(base); //Checks if tamu.edu is at end of base url
	if (baseCheck.find("tamu.edu\0") != -1) {
		FromT++;
	}
	for (int i = 0; i < nLinks; i++) {
		modifURL tamuCheck(linkBuf);
		//checks if hyperlink is a tamu link
		if (tamuCheck.gethostName().find("tamu.edu") != -1) { 
			tamuCount++;
		}
		linkBuf += strlen(linkBuf) + 1; //Pulling hyperlinks form linkbuffer
	}
	WaitForSingleObject(p->ParamMutex, INFINITE);
	p->TamuLinks += tamuCount; //Add values to shared memory at end
	p->FromTamu += FromT;
	ReleaseMutex(p->ParamMutex);
}