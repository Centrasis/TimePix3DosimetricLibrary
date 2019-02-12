#pragma once

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/time.h>
#define SOCKETTYPE int
#define SOCKET_ADDR_T struct sockaddr_in
#define CLOSESOCK(sock) close(sock)
#define INITWSDATA()
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
static inline int TIMEOUT_MAKRO(int sock, uint32_t timeout_ms) {
	struct timeval val;
	val.tv_sec = timeout_ms / 1000.f;
	val.tv_usec = 1000 * (timeout_ms % 1000);
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &val, sizeof(struct timeval));
}
#define SETTIMEOUT(sock, timeout_ms) TIMEOUT_MAKRO(sock, timeout_ms)
#define OUTPUT_SOCKET_ERROR()

#else
#define _WINSOCKAPI_
#include "MultiplattformTypes.h"
#include <WinSock2.h>
#define SOCKETTYPE SOCKET
#define SOCKET_ADDR_T SOCKADDR_IN
#define CLOSESOCK(sock) if(sock > 0) closesocket(sock)

static inline int TIMEOUT_MAKRO(SOCKET sock, uint32_t timeout_ms)
{
	char* timeout = (char*)&timeout_ms;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(uint32_t));
}
#define SETTIMEOUT(sock, timeout_ms) TIMEOUT_MAKRO(sock, timeout_ms)

static inline void OUTPUT_SOCKET_ERROR(int code)
{
	char* out;
	switch (code)
	{
	case WSANOTINITIALISED: out = "Not Initialized"; break;
	case WSAENETDOWN: out = "Subsystem fail"; break;
	case WSAEFAULT: out = "Buffer invalid memory"; break;
	case WSAEINVAL: out = "Server setup incorrect"; break;
	case WSAEISCONN: out = "Socket is connected, but should not"; break;
	case WSAENETRESET: out = "UDP expired"; break;
	case WSAENOTSOCK: out = "Wrong descriptor"; break;
	case WSAEOPNOTSUPP: out = "Socket not supported"; break;
	case WSAESHUTDOWN: out = "Socket already shuted down"; break;
	case WSAEWOULDBLOCK: out = "Socket can't block"; break;
	case WSAEMSGSIZE: out = "Message too large"; break;
	case WSAETIMEDOUT: out = "Socket timed out"; break;
	case WSAECONNRESET: out = "Connection was reseted"; break;
	default: out = "Unknown error code";
	}

	out;
}

#define OUTPUT_SOCKET_ERROR() OUTPUT_SOCKET_ERROR(WSAGetLastError())

//Overwritten in udp.c
#define INITWSDATA()
#endif