#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "handshake.h"
#include "ftransfer.h"
#include "helper.h"
#include "util.h"


int main(int argc, char **argv)
{
	int sockfd, filefd;
	int exstat;
	int inbytes, outbytes;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t servAddrLen, cliAddrLen;
	hostinfo_t hinfo;
	char *ip;
	conninfo_t self, other;
	uint16_t seq, ack;
	size_t fsize;

	servAddrLen = sizeof(struct sockaddr_storage);
	cliAddrLen = sizeof(struct sockaddr_storage);

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
	servAddr.sin_addr.s_addr = inet_addr("10.0.0.1");
	memset(servAddr.sin_zero, 0, 8);
	servAddrLen = sizeof(struct sockaddr);
	if ((exstat = bind(sockfd, (struct sockaddr *)&servAddr, servAddrLen) != 0)) {
		fprintf(stderr, "Error: cannot bind to socket\n");
		return exstat;
	}

	hinfo.sockfd = sockfd;
	hinfo.addr = &cliAddr;
	hinfo.addrlen = cliAddrLen;
	self.rwnd = INITRWND;

	// prepare to send the file
	if ((filefd = open(argv[2], O_RDONLY)) < 0) {
		fprintf(stderr, "Error: cannot open file '%s'\n", argv[2]);
		return 1;
	}
	fsize = lseek(filefd, 0, SEEK_END);
	lseek(filefd, 0, SEEK_SET);
	// use seq and ack field to store file size
	self.seq = fsize >> 16;
	self.ack = fsize;
	// wait for a client to initiate three-way handshake
	if (handshake_server(&hinfo, &self, &other)) {
		fprintf(stderr, "Error: cannot set up a TCP-like connection\n");
		return 1;
	}


	if (ftransfer_sender(&hinfo, filefd, fsize, &self, &other)) {
		close(filefd);
		fprintf(stderr, "Error transfering file, exiting.\n");
		return 1;
	}

	if (terminate_server(&hinfo, &self, &other)) {
		fprintf(stderr, "Error terminating connection with the client via handshaking, force exiting\n");
	}

	// clean up


	return 0;
}