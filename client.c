#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include "handshake.h"
#include "helper.h"
#include "util.h"


int main(int argc, char **argv)
{
	int sockfd;
	int exstat;
	int inbytes, outbytes;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t servAddrLen, cliAddrLen;
	hostinfo_t hinfo;
	char *ip;
	conninfo_t self, other;
	uint16_t seq, ack;

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

	// set server location information and tcp header variables
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[2]));
	servAddr.sin_addr.s_addr = inet_addr(ip);
	memset(servAddr.sin_zero, 0, 8);
	servAddrLen = sizeof(servAddr);

	hinfo.sockfd = sockfd;
	hinfo.addr = &servAddr;
	hinfo.addrlen = servAddrLen;
	self.rwnd = INITRWND;

	// set up handshake data
	if (handshake_client(&hinfo, &self, &other)) {
		fprintf(stderr, "Error: cannot set up a TCP-like connection\n");
		return 1;
	}

	unsigned char buf[1032];
	// wait for data
	while (recv(sockfd, buf, 1032, 0) < 0) {
		int i;
		for (i = 0; i < 8; i++) {
			printf("%.2x", buf[i]);
		}
		printf("\n");
		for (i = 0; i < 1024; i++) {
			if (i != 0 && i % 16 == 0)
				printf("\n");
			printf("%.2x\n", buf[i+8]);
		}
		printf("\n");
	}

	return 213;
}