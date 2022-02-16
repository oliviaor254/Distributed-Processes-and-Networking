//Checksum.h: Header file for checksum object
#pragma once
#include "pch.h"

class Checksum {
private:
	DWORD crc_table[256]; //Try unsigned long if DWORD doesn't work
public:
	Checksum();
	u_short ip_checksum(u_short* buffer, int size);
};