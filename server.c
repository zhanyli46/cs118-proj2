#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
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

	// check program arguments
	if (argc != 3) {
		fprintf(stderr, "Usage: ./server PORT-NUMBER FILE-NAME\n");
		return 0;
	}
	if (!is_numeric(argv[1]) || (access(argv[2], F_OK) != 0)) {
		fprintf(stderr, "Error: port number invalid or file does not exist\n");
		return 1;
	}

	// try to create/bind socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Error: cannot create socket\n");
		return sockfd;
	}
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[1]));
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(servAddr.sin_zero, 0, 8);
	servAddrLen = sizeof(struct sockaddr);
	if ((exstat = bind(sockfd, (struct sockaddr *)&servAddr, servAddrLen) != 0)) {
		fprintf(stderr, "Error: cannot bind to socket\n");
		return exstat;
	}

	// wait for a client to initiate three-way handshake
	if ((inbytes = recv(sockfd, inbuf, BUFSIZE, 0) > 0)) {
		fprintf(stdout, "%s\n", inbuf);
	}


	return 0;
}