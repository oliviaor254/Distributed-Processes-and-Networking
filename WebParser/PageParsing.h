#pragma once
#ifndef PageParsing_h
#define PageParsing_h
#include "pch.h"
#include "modifURL.h"
#include "SocketM.h"
#include "HTMLParserBase.h"
#include "Parameters.h"
#include <unordered_set>
class PageParsing {
public:
	void parse(char* url, LPVOID pParam);
	char* buildGETRequest(char* host, char* port, char* request);
	char* parseURLS();

};
#endif