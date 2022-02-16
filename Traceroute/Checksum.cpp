//Checksum.cpp: Function used to calculate ICMP checksums
#include "pch.h"
#include "Checksum.h"

Checksum::Checksum() {
	// set up a lookup table for later use
	for (DWORD i = 0; i < 256; i++)
	{
		DWORD c = i;
		for (int j = 0; j < 8; j++) {
			c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
		}
		crc_table[i] = c;
	}
}

u_short Checksum::ip_checksum(u_short* buffer, int size)
{
	u_long cksum = 0;
	/* sum all the words together, adding the final byte if size is odd */
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(u_short);
	}
	if (size)
		cksum += *(u_char*)buffer;

	/* add carry bits to lower u_short word */
	cksum = (cksum >> 16) + (cksum & 0xffff);

	/* return a bitwise complement of the resulting mishmash */
	return (u_short)(~cksum);
}