#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

#define BUFSIZE 1024

int main(int argc, char **argv)
{
	int sockfd;
	int exstat;
	int inbytes, outbytes;
	char inbuf[BUFSIZE], outbuf[BUFSIZE];
	struct sockaddr_in servAddr, cliAddr;
	socklen_t servAddrLen, cliAddrLen;
	char *ip;


	// check program arguments
	if (argc != 3) {
		fprintf(stderr, "Usage: ./server SERVER-HOST-OR-IP PORT-NUMBER\n");
		return 0;
	}
	if (!is_numeric(argv[2])) {
		fprintf(stderr, "Error: invalid port number\n");
		return 1;
	}
	if (convert_to_ip(argv[1], ip) != 1) {
		fprintf(stderr, "Error: cannot resolve %s\n", argv[1]);
		return 1;
	}
exit(0);

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

	// set info for testing !
	strcpy(outbuf, "this is a test message; will be replaced by actual packet later.");

	// try to set up TCP-like connection
	if ((outbytes = sendto(sockfd, outbuf, BUFSIZE, 0, (struct sockaddr *)&servAddr, servAddrLen)) == -1) {
		fprintf(stderr, "Error: unable to send packets to indicated server\n");
		return outbytes;
	}

	return 0;
}