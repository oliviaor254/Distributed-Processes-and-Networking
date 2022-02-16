/*Hw1Part3.cpp: Completed assignment of Web Crawler
Created by: Olivia Ornelas
* Class: CSCE 463, section 500
* Semester: Spring 2021
* Homework: Hw1 part 3
* Date: 02/12/2021
*/
#include "pch.h"
#include <unordered_set>
#include<queue>
#include "modifURL.h"
#include "SocketM.h"
#include "HTMLParserBase.h"
#include "Parameters.h"

void initializeParams(LPVOID lpParam, int numT, char* filename) {
	Parameters* p = (Parameters*)lpParam;
	p->Clock = clock(); //Keeps track of when program started
	p->ParamMutex = CreateMutex(NULL, 0, NULL);
	p->eventQuit = CreateEvent(NULL, true, false, NULL);
	p->FileUrls = queue<char*>();
	p->seenHosts = unordered_set<string>();
	p->seenIps = unordered_set<DWORD>();
	p->Filename = filename;
	p->numThreads = 0;
}
//(Producer thread)
DWORD WINAPI fileThreadFunct(LPVOID pParams) {
	Parameters* fileParams = (Parameters*)pParams;
	string fileLine;
	WaitForSingleObject(fileParams->ParamMutex, INFINITE);
	// open html file
	HANDLE hFile = CreateFileA(fileParams->Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	ReleaseMutex(fileParams->ParamMutex); //Done with shared memory section
	// process errors
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("CreateFile failed with %d\n", GetLastError());
		return 0;
	}
	// get file size
	LARGE_INTEGER li;
	BOOL bRet = GetFileSizeEx(hFile, &li);
	// process errors
	if (bRet == 0)
	{
		printf("GetFileSizeEx error %d\n", GetLastError());
		return 0;
	}
	// read file into a buffer
	__int64 fileSize = (DWORD)li.QuadPart;
	printf("File: %s successfully opened with %I64u bytes\n", fileParams->Filename,fileSize);
	DWORD bytesRead;
	char* fileBuf = new char[fileSize];
	// read into the buffer
	bRet = ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL);
	// process errors
	if (bRet == 0 || bytesRead != fileSize)
	{
		printf("ReadFile failed with %d\n", GetLastError());
		return 0;
	}
	// done with the file
	CloseHandle(hFile);
	char* find;
	__int64 pos = -1;
	int count = 0;
	string s;
	vector<char*> UrLs = vector<char*>();
	//Extracting URLs from buffer
	find = strchr(fileBuf, '\r\n');
	while (find != NULL) {
		char temp[5000];
		pos = find - fileBuf; //Subtracting fileBuf since find is a string
		strncpy_s(temp, 5000, fileBuf, pos);
		s = temp;
		UrLs.push_back(_strdup(s.c_str()));
		fileBuf = find + 2;
		find = strchr(fileBuf, '\r\n');
	}
	UrLs.push_back(fileBuf); //Gets last url in buffer
	for (int a = 0; a < UrLs.size(); a++) { //After reading into a vector, urls are safely placed into the shared memory queue
		WaitForSingleObject(fileParams->ParamMutex, INFINITE);
		fileParams->FileUrls.push(UrLs[a]);
		ReleaseMutex(fileParams->ParamMutex);
	}
	return 0;
}
DWORD WINAPI statThreadFunction(LPVOID lpParam) {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	Parameters* statP = (Parameters*)lpParam;
	clock_t prevClock = clock();
	double StatSpeed = 0.0;
	double byteSpeed = 0.0;
	double SecsLastReport = 0.0;
	double totalSecs = 0.0;
	while (WaitForSingleObject(statP->eventQuit, 2000) == WAIT_TIMEOUT) { //Prints stats every 2 seconds
		double t1 = (clock() - prevClock)/ CLOCKS_PER_SEC;
		SecsLastReport = floor(t1);
		double total = (prevClock - statP->Clock) / CLOCKS_PER_SEC; 
		totalSecs = floor(total); 
		printf("[%3d] %5d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n",(int)totalSecs, statP->numThreads,(int)statP->FileUrls.size(), statP->ExtractedURLCount, statP->numUniqueHosts,
		statP->SuccessfulDNS, statP->numUniqueIPs, statP->numRobotPassed, statP->CrawledCount, statP->numLinks/1000);
		StatSpeed = statP->CrawledCount / SecsLastReport;
		byteSpeed = (statP->BytesDownloaded/1000000)/ SecsLastReport; //Char = 1byte, Mbps = Megabits per sec.
		printf("\t *** crawling %.1f pps @ %.1f Mbps\n", StatSpeed, byteSpeed);
		prevClock = clock();
	}
	//Printing summary since threads are at EventQuit
	WaitForSingleObject(statP->ParamMutex, INFINITE);
	totalSecs = (clock() - statP->Clock) / CLOCKS_PER_SEC;
	printf("\n\tExtracted %d URLs @ %d/s\n", statP->ExtractedURLCount, (int)(statP->ExtractedURLCount / totalSecs));
	printf("\tLooked up %d DNS names @ %d/s\n", statP->SuccessfulDNS, (int)(statP->SuccessfulDNS / totalSecs)); //: Double check if int or double
	printf("\tAttempted %d robots @ %d/s\n", statP->numUniqueIPs, (int)(statP->numUniqueIPs / totalSecs));
	printf("\tCrawled %d pages @ %d/s (%.2f MB)\n", statP->CrawledCount, (int)(statP->CrawledCount / totalSecs), (float)(statP->BytesDownloaded / 1000000.0));
	printf("\tParsed %d links @ %d/s\n", statP->numLinks, (int)(statP->numLinks / totalSecs));
	printf("\tHTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, Other = %d\n", statP->Status2xx, statP->Status3xx, statP->Status4xx, statP->Status5xx, statP->OtherStatus);
	printf("\tTamuLinks = %d\t From Tamu = %d\n", statP->TamuLinks,statP->FromTamu);
	ReleaseMutex(statP->ParamMutex);
	return 0;
}

//Consumers
DWORD WINAPI crawlerThreadFunction(LPVOID lpParam) {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
	Parameters* crawlParams = ((Parameters*)lpParam);
	HTMLParserBase* parse1 = new HTMLParserBase;
	char* urlStr; 
	while (true) {
		//Get 1 URL per thread
		WaitForSingleObject(crawlParams->ParamMutex, INFINITE);
		if (crawlParams->FileUrls.size() == 0) {
			crawlParams->numThreads--;
			ReleaseMutex(crawlParams->ParamMutex);
			break;
		}

		urlStr = crawlParams->FileUrls.front();
		crawlParams->FileUrls.pop();
		crawlParams->ExtractedURLCount += 1;
		ReleaseMutex(crawlParams->ParamMutex);
		//Creates a modifURL object that parses the url and has a function(parse) that sends requests and parses the response
		modifURL CrawlingObj = modifURL(urlStr);
		CrawlingObj.parse(lpParam,urlStr, parse1);
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int numThreads = 0;
	if ((argc < 3) || (argc > 3)) {
		printf("Invalid number of arguments: [# of threads] [Filename]\n");
		return 0;
	}
	else {
		numThreads = atoi(argv[1]);
		Parameters p;
		initializeParams(&p, numThreads, argv[2]);
		//Start filethread
		HANDLE fileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fileThreadFunct, &p, 0, NULL);
		//Close filethread after gathering urls into shared queue
		if (fileThread != 0) {
			WaitForSingleObject(fileThread, INFINITE);
			CloseHandle(fileThread);
		}
		//Starts crawlers
		HANDLE* crawlerThreads = new HANDLE[numThreads];
		for (int i = 0; i < numThreads; i++) {
			InterlockedIncrement((LONG*)&p.numThreads);
			crawlerThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawlerThreadFunction, &p, 0, NULL);
		}
		//Start stats
		HANDLE statThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statThreadFunction, &p, 0, NULL);
		//Close crawlers
		for (int i = 0; i < numThreads; i++) {
			if (crawlerThreads[i] != 0) {
				WaitForSingleObject(crawlerThreads[i], INFINITE);
				CloseHandle(crawlerThreads[i]);
			}
		}
		SetEvent(p.eventQuit);
		//Close stats after printing summary message
		if (statThread != 0) {
			WaitForSingleObject(statThread, INFINITE);
			CloseHandle(statThread);
		}
	}
	return 0;
}