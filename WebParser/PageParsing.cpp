#include "pch.h"
#include "PageParsing.h"
//WHAT NEEDS TO BE FIXED
//Constructor..
PageParsing::PageParsing(char* url, LPVOID pParam) { //DEBUG:Make parameters a separate class 
	Parameters* p = (Parameters*)pParam;
	//DEBUG: THE ACTUAL EXECUTION OF THE PARSING!! - Was in the main
	for (int a = 0; a < p->FileUrls.size(); a++) { //Needs to be fixed
		SocketM sock1 = SocketM();
		modifURL url(p->FileUrls.at(a));
		if (performCrawl(&sock1, url, true, &p->hosts, &p->seenIps, "HEAD")) //See if this can be better
		{
			IN_ADDR prevIP = sock1.getAddr();
			SocketM sock2 = SocketM();
			sock2.setAddr(prevIP, url);
			performCrawl(&sock2, url, false, &p->hosts, &p->seenIps, "GET");
		}
	}
}


bool PageParsing::performCrawl(SocketM* sock, modifURL ParsedL, bool robotPres, unordered_set<string>* hosts, unordered_set<DWORD>* seenIps, string method) {
	//First request
	if (robotPres) {
		printf("\t  Checking host uniquenes...");
		size_t HostSize = hosts->size();
		hosts->insert(ParsedL.gethostName());
		if (HostSize == hosts->size()) {
			printf("failed\n");
			return false;
		}
		printf("passed\n");
		if (sock->DNSLookup(ParsedL) == 0)
			return false;
		printf("\t  Checking IP Uniqueness....");
		size_t prevSize = seenIps->size();
		seenIps->insert(sock->getAddr().S_un.S_addr);
		if (seenIps->size() == prevSize) {
			printf("failed\n");
			return false;
		}
		else {
			printf("passed\n");
		}
	}
	//Same for each request
	if (sock->ConnectandSend(ParsedL, robotPres))
	{
		string respstr(sock->getbuf()); //Easier to parse as string
		int statusCode = checkHeader(respstr, robotPres);
		if (statusCode == -1) {
			return false;
		}
		if (strcmp(method.c_str(), "GET") == 0) {
			if ((statusCode < 300) && (statusCode >= 200)) {
				PageParser(ParsedL, sock->getbuf(), sock->getBufferSize(), false);
				return true;
			}
		}
		return true; //Valid Robot status code
	}
	return false;
}

int PageParsing::checkHeader(string header, bool robos) {
	printf("\t  Verifying Header...");
	if (strlen(header.c_str()) <= 0) {
		printf("HTML error, bad request: %d\n", WSAGetLastError());
		return -1;
	}
	int status = atoi(header.substr(9, 3).c_str()); //known spot because of protocol
	if (status <= 99) {
		printf("failed due to non-HTTP header\n");
		return -1;
	}
	printf("status Code: %d\n", status);
	if (!robos) {
		if ((status < 200) || (status >= 300)) {
			return -1;
		}
	}
	else if (status < 400) { //If robot request and status is < 400	
		return -1;
	}
	return status;
}
int PageParser(modifURL syntax, char* pagebuf, int bufSize, bool robotsPhase) {
	Parameters *p = new Parameters();
	int nlinks;
	HTMLParserBase* parser = new HTMLParserBase;
	char* BaseUrl = syntax.getBase();
	char* linkBuffer = parser->Parse(pagebuf, bufSize, BaseUrl, (int)strlen(BaseUrl), &nlinks);
	int tamuCount = 0;
	string tamuCheck;
	bool istamu = false;
	for (int i = 0; i < nlinks; i++) {
		tamuCheck = string(linkBuffer);
		istamu = isTamuL(tamuCheck);
		if (istamu)
			tamuCount++;
		linkBuffer += strlen(linkBuffer) + 1; //DEBUG: This seems WRONG
	}
	if (tamuCount > 0) {
		//Lock thread
		WaitForSingleObject(p->Mutex, INFINITE); //DEBUG: staticMutex = HANDLE staticMutex in struct parameters - thread paramteters
		if (istamu)
			p->tamu_internal += tamuCount;
		else
			p->tamu_external += tamuCount; //DEBUG: Keeps track of how many not tamu for every thread
		//Unlock thread
		ReleaseMutex(p->Mutex); //DEBUG: staticMutex = Thread's mutex
	}
	delete BaseUrl; //DEBUG: Correct?
	return nlinks;
}

bool isTamuL(string s) {
	int tamuIndx = s.find("tamu.edu");
	if ((tamuIndx != string::npos) || (s.find("tamu.edu.") == string::npos)) {
		return false;
	}
	return true;
}