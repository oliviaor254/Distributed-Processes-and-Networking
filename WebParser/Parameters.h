#ifndef Parameters_h
#define Parameters_h
#include "pch.h"
#include <unordered_set>
#include <vector>
#include <queue>

//Placed in headerfile so can access shared memory in other classes (not just main)
class Parameters {
public:
	HANDLE	ParamMutex;
	HANDLE countMutex;
	HANDLE	eventQuit;
	HANDLE eventFileRead;
	HANDLE semaphoreCrawlers;
	HANDLE queueMutex;

	queue<char*> FileUrls;
	unordered_set<string> seenHosts;
	unordered_set<DWORD> seenIps;
	
	char* Filename;
	clock_t Clock = clock(); //Signify start of program

	int TamuLinks = 0;
	int FromTamu = 0;
	double BytesDownloaded = 0; //Combined bytes of Robot & regular pages
	int Status2xx = 0;
	int Status3xx = 0;
	int Status4xx = 0;
	int Status5xx = 0;
	int OtherStatus = 0;
	int ExtractedURLCount = 0;
	int numUniqueHosts = 0;
	int SuccessfulDNS = 0;
	int numUniqueIPs = 0;
	int numRobotPassed = 0;
	int CrawledCount = 0;
	int numLinks = 0;
	int numThreads = 0;

};
#endif