/* Name: Olivia Ornelas
*  Date: 05/03/2021
*  Class: CSCE 463
* Hw4.cpp: Main file used to perform a traceroute using 'ping' and ICMP packets.
* Produces threads to perform reverse DNS lookups while also receiving packets.
* Also calculates time program runs in nanosecond precision.
*/

#include "pch.h"
#include "PktHdrs.h"
#include "Checksum.h"
#include <thread>
#include <queue>
int findIdx(vector<PrintHdr> toFind, u_short ttL) {
	for (int a = 0; a < toFind.size(); a++) {
		if (toFind.at(a).TTLval == ttL) {
			return a;
		}
	}
	return -1; //Couldn't find it
}

DWORD WINAPI DNSlookups(PrintHdr* lparam) { //DNS lookup threads
	PrintHdr* p = lparam;
	struct hostent* host_entry = NULL;
	struct in_addr addr;
	addr.S_un.S_addr = htonl(p->IP);
	host_entry = gethostbyaddr((char*)&addr, 4, AF_INET);
	if (host_entry == NULL) {
		int ErrorNo = WSAGetLastError();
		if (ErrorNo != 0) {
			if ((ErrorNo == WSAHOST_NOT_FOUND) || (ErrorNo == WSANO_DATA)) {
				char* hs = (char*)"<No DNS entry>\0";
				memcpy(p->hostname, hs, strlen(hs));
				return 1;
			}
			printf("DNS function failed with %d\n", ErrorNo);
			return 1;
		}
	}
	memcpy(p->hostname, host_entry->h_name, strlen(host_entry->h_name)); //Covers if host_entry is above
	return 0;
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Usage error: Hw4.exe [hostname]\n");
		return -1;
	}
	char* hostname = argv[1];
	vector<PrintHdr> resultsBuf;
	resultsBuf.reserve(30);
	struct hostent* remote;
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	memset(&remote, 0, sizeof(remote));
	DWORD IP = inet_addr(hostname);
	LARGE_INTEGER start, end, total, Frequency;
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}
	HANDLE SocketRecvReady = CreateEvent(NULL, FALSE, FALSE, NULL);
	SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock == INVALID_SOCKET) {
		printf("Unable to create a raw socket: error %d\n", WSAGetLastError());
		return -1;
	}
	WSAEventSelect(sock, SocketRecvReady, FD_READ);
	if (IP == INADDR_NONE)
	{
		if ((remote = gethostbyname(hostname)) == NULL)
		{
			printf("Invalid string: neither FQDN, nor IP address\n");
			return -1;
		}
		else
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		server.sin_addr.S_un.S_addr = IP;
	}
	IP = server.sin_addr.S_un.S_addr; //IP to be used in function
	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	printf("Tracerouting to %s...\n", inet_ntoa(server.sin_addr));
	u_char send_buf[MAX_ICMP_SIZE];	//Buffer for ICMP Header
	ICMPHeader* icmp = (ICMPHeader*)send_buf;
	//Set up echo request - no need to flip the byte order since fields are 1 byte each
	icmp->type = ICMP_ECHO_REQUEST;
	icmp->code = 0;
	icmp->id = (u_short)GetCurrentProcessId();
	icmp->seq = 0;
	int packet_size = sizeof(ICMPHeader);
	int ttl = 0;
	//Sending
	for (int i = 0; i < 30; i++) {
		PrintHdr ProbeInsert;
		ttl++;
		ProbeInsert.TTLval = ttl;
		ProbeInsert.probes = 0;
		memset(ProbeInsert.hostname, 0, NI_MAXHOST);
		memset(&ProbeInsert.IP, 0, sizeof(ULONG));
		icmp->seq = ttl;
		icmp->checksum = 0;
		Checksum cs;
		DWORD check = cs.ip_checksum((u_short*)send_buf, packet_size);
		icmp->checksum = check; //Recomputing checksum
		if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
			printf("setsockopt failed with %d\n", WSAGetLastError());
			closesocket(sock);
			return -1;
		}
		QueryPerformanceCounter(&ProbeInsert.personalStart);
		QueryPerformanceCounter(&start);
		sendto(sock, (char*)&send_buf, packet_size, NULL, (struct sockaddr*)&server, sizeof(server));
		ProbeInsert.probes = ProbeInsert.probes + 1;
		resultsBuf.push_back(ProbeInsert);
	}
	QueryPerformanceFrequency(&Frequency);
	int i = 0; //Used to count how many loops have happened
	thread dnsLookups[30]; //Using stl threads so main can wait on DNS threads with .join()
	fd_set fds;
	int avail = -1;
	int found = -1;
	int Idx = -1;
	long double Interval = (long double(1)) / (Frequency.QuadPart); //QPC implementation follows https://gist.github.com/rdp/1153062 
	bool allFound = false;
	//Receiving
	double RTo = 0.5; //Initial timeout set to .5s
	while (true) { //Will only end if there is nothing left to read (avail = 0)
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		timeval to;
		//Check current time 
		to.tv_sec = ceil(RTo);
		to.tv_usec = (RTo - ceil(RTo)) * 1000;
		avail = select(0, &fds, NULL, NULL, &to);
		if (avail <= 0) {
			//Retx
			for (int a = 0; a < resultsBuf.size(); a++) { //Will iterate 30 times
				if (resultsBuf[a].IP == 0) { //Router IP hasn't been assigned
					if (resultsBuf[a].probes > 3) {
						memcpy(resultsBuf[a].hostname, "*\0", 2);
						continue;
					}
					icmp->seq = resultsBuf[a].TTLval;
					icmp->checksum = 0;
					Checksum cs;
					DWORD check = cs.ip_checksum((u_short*)send_buf, packet_size);
					icmp->checksum = check; //Recomputing checksum
					if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&resultsBuf[a].TTLval, sizeof(resultsBuf[a].TTLval)) == SOCKET_ERROR) {
						printf("setsockopt failed with %d\n", WSAGetLastError());
						closesocket(sock);
						return -1;
					}
					sendto(sock, (char*)&send_buf, packet_size, NULL, (struct sockaddr*)&server, sizeof(server));
					resultsBuf[a].probes++;
					//Calculate new RTO
					if (a < resultsBuf.size() - 1) {
						if ((resultsBuf[a - 1].IP != 0) && (resultsBuf[a + 1].IP != 0)) {
							RTo = resultsBuf[a - 1].RTT + resultsBuf[a + 1].RTT;
							RTo /= 2.0;
						}
					}
				}
			}
			if ((i >= found) && (WaitForSingleObject(SocketRecvReady, 2000) == WAIT_TIMEOUT)) { //Stop if destination has been reached & no more retrx are happening (nothing left to read)
				break;
			}
			continue;
		}
		else {
			u_char recv_buf[MAX_REPLY_SIZE];
			IPHeader* router_ip_hdr = (IPHeader*)(recv_buf);
			ICMPHeader* router_icmp_hdr = (ICMPHeader*)(router_ip_hdr + 1);
			IPHeader* orig_ip_hdr = (IPHeader*)(router_icmp_hdr + 1);
			ICMPHeader* orig_icmp_hdr = (ICMPHeader*)(orig_ip_hdr + 1);
			struct sockaddr_in resp;
			int respSz = sizeof(resp);
			int recvBytes = recvfrom(sock, (char*)recv_buf, MAX_REPLY_SIZE, NULL, (struct sockaddr*)&resp, &respSz);
			if (recvBytes == SOCKET_ERROR) {
				printf("Error on recvfrom %d\n", WSAGetLastError());
				return -1;
			}
			if (recvBytes > 56) { //Variable - Larger Header
				router_icmp_hdr = (ICMPHeader*)(recv_buf + (router_ip_hdr->h_len * 4));
				orig_ip_hdr = (IPHeader*)(router_icmp_hdr + 1);
				orig_icmp_hdr = (ICMPHeader*)(orig_ip_hdr + 1);
			}
			if ((router_icmp_hdr->type == ICMP_TTL_EXPIRED) && (router_icmp_hdr->code == 0) && (recvBytes >= 56))
			{
				if (orig_ip_hdr->proto == 1) {
					if (orig_icmp_hdr->id == GetCurrentProcessId()) { //Checks since not at destination
						Idx = findIdx(resultsBuf, orig_icmp_hdr->seq);
						if (Idx == -1) {
							printf("Error out of bounds hop #\n");
							return -1;
						}
						PrintHdr* prinh = &resultsBuf.at(Idx);
						QueryPerformanceCounter(&prinh->personalEnd);
						prinh->TTLval = orig_icmp_hdr->seq;
						prinh->RTT = long double(prinh->personalEnd.QuadPart - prinh->personalStart.QuadPart);
						prinh->RTT *= Interval;
						prinh->IP = ntohl(router_ip_hdr->source_ip);
						dnsLookups[Idx] = thread(DNSlookups, &resultsBuf[Idx]);
						i++;
					}
				}
			}
			else if ((router_icmp_hdr->type == ICMP_ECHO_REPLY) || (router_icmp_hdr->type == ICMP_DEST_UNREACH)) {
				if (ntohs(router_ip_hdr->len) == 28) {
					if (found == -1) { //Insert if its the first instance of reaching the destination
						found = router_icmp_hdr->seq; //Same as sequence
						Idx = findIdx(resultsBuf, router_icmp_hdr->seq);
						if (Idx == -1) {
							printf("Error out of bounds hop #\n");
							continue; //Continue
						}
						PrintHdr* prinh = &resultsBuf.at(Idx);
						QueryPerformanceCounter(&prinh->personalEnd);
						prinh->TTLval = orig_icmp_hdr->seq;
						prinh->RTT = long double(prinh->personalEnd.QuadPart - prinh->personalStart.QuadPart);
						prinh->RTT *= Interval;
						prinh->TTLval = router_icmp_hdr->seq;
						prinh->IP = ntohl(router_ip_hdr->source_ip);
						dnsLookups[Idx] = thread(DNSlookups, &resultsBuf[Idx]);	//Set Total seconds here
					}
					i++;
				}
			}
			else if ((recvBytes >= 56) && (orig_icmp_hdr->id == GetCurrentProcessId()) && (orig_ip_hdr->proto == 1)) {
				Idx = findIdx(resultsBuf, orig_icmp_hdr->seq);
				if (Idx == -1) {
					printf("Error out of bounds hop #\n");
					continue;
				}
				PrintHdr* prinh = &resultsBuf.at(Idx);
				sprintf_s(prinh->hostname, "other error: code %d type %d", router_icmp_hdr->code, router_icmp_hdr->type);
			}
		}

	}
	for (int a = 0; a < found; a++) { //Makes main thread wait on DNS threads before continuing
		if (dnsLookups[a].joinable()) //Checks if the thread was opened
			dnsLookups[a].join();
	}
	QueryPerformanceCounter(&end);
	long double TotalExec = long double(end.QuadPart - start.QuadPart);
	TotalExec *= Interval;
	struct in_addr prinIP; //Used for printing IPs
	for (int d = 0; d < found; d++) {
		if (resultsBuf[d].IP == 0) {
			printf(" %2u %s\n", resultsBuf[d].TTLval, resultsBuf[d].hostname); //Also covers if ICMP error
			continue;
		}
		prinIP.S_un.S_addr = htonl(resultsBuf[d].IP);
		printf(" %2u %s (%s) %0.3Lf ms (%d)\n", resultsBuf[d].TTLval, resultsBuf[d].hostname, inet_ntoa(prinIP), resultsBuf[d].RTT, resultsBuf[d].probes);
	}
	printf("Total execution time %0.3Lf ms\n", TotalExec);
	WSACleanup();
	return 0;
}
