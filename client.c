#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"
#include "util.h"

#define RWND 30720


int main(int argc, char **argv)
{
	int sockfd;
	int exstat;
	int inbytes, outbytes;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t servAddrLen, cliAddrLen;
	char *ip;
	uint16_t seq, ack;
	uint16_t rwnd = RWND;
	// check program arguments
	if (argc != 3) {
		fprintf(stderr, "Usage: ./server SERVER-HOST-OR-IP PORT-NUMBER\n");
		return 0;
	}
	if (!is_numeric(argv[2])) {
		fprintf(stderr, "Error: invalid port number\n");
		return 1;
	}
	if (convert_to_ip(argv[1], &ip) != 1) {
		fprintf(stderr, "Error: cannot resolve %s\n", argv[1]);
		return 1;
	}

	// try to create socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Error: cannot create socket\n");
		return sockfd;
	}

	// set server location information
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[2]));
	servAddr.sin_addr.s_addr = inet_addr(ip);
	memset(servAddr.sin_zero, 0, 8);
	servAddrLen = sizeof(servAddr);

	// set up handshake data
	if (handshake_client(&seq, &ack, rwnd) != 1) {
		fprintf(stderr, "Error: cannot set up a TCP-like connection\n");
		return 1;
	}


	return 0;
}