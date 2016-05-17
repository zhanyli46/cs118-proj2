#include <string.h>
#include "helper.h"
#include "util.h"

#include <stdio.h>

int handshake_client(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other)
{	
	unsigned char packet[BUFSIZE];
	uint16_t flag;
	ssize_t outbytes;

	// specify TCP-like connection information
	memset(packet, 0, BUFSIZE);
	self->seq = init_seqnum();
	self->ack = 0;
	flag = SYN;

	fill_header(packet, &self->seq, &self->ack, &self->rwnd, &flag);
	
	// testing
	printf("self: %hu %hu %hu %hu\n", self->seq, self->ack, self->rwnd, flag);
	int i;
	for (i = 0; i < 8; i++) {
		printf("%.2x", packet[i]);
	}
	printf("\n");
	// testing

	// send SYN with sequence number
	if ((outbytes = sendto(hinfo->sockfd, packet, BUFSIZE, 0, (struct sockaddr *)hinfo->addr, hinfo->addrlen)) < 0)
		return 0;


	
	return 0;
}

int handshake_server(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other)
{
	unsigned char packet[BUFSIZE];
	uint16_t flag;
	ssize_t inbytes;

	memset(packet, 0, BUFSIZE);
	self->seq = init_seqnum();

	if ((inbytes = recvfrom(hinfo->sockfd, packet, BUFSIZE, 0, (struct sockaddr *)hinfo->addr, &hinfo->addrlen) > 0)) {
		interpret_header(packet, &other->seq, &other->ack, &other->rwnd, &flag);
		
		// testing	
		printf("other: %hu %hu %hu %hu\n", other->seq, other->ack, other->rwnd, flag);
		int i;
		for (i = 0; i < 8; i++) {
			printf("%.2x", packet[i]);
		}
		printf("\n");
		// testing
	
	}
	return 0;
}


void fill_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag)
{
	unsigned char sseq[2], sack[2], srwnd[2], sflag[2];
	ushort_to_string(seq, sseq);
	ushort_to_string(ack, sack);
	ushort_to_string(rwnd, srwnd);
	ushort_to_string(flag, sflag);

	memcpy(p, sseq, 1);
	memcpy(p+1, sseq+1, 1);
	memcpy(p+2, sack, 1);
	memcpy(p+3, sack+1, 1);
	memcpy(p+4, srwnd, 1);
	memcpy(p+5, srwnd+1, 1);
	memcpy(p+6, sflag, 1);
	memcpy(p+7, sflag+1, 1);

	return;
}

void interpret_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag)
{
	unsigned char sseq[2], sack[2], srwnd[2], sflag[2];
	memcpy(sseq, p, 1);
	memcpy(sseq+1, p+1, 1);
	memcpy(sack, p+2, 1);
	memcpy(sack+1, p+3, 1);
	memcpy(srwnd, p+4, 1);
	memcpy(srwnd+1, p+5, 1);
	memcpy(sflag, p+6, 1);
	memcpy(sflag+1, p+7, 1);
	

	string_to_ushort(sseq, seq);
	string_to_ushort(sack, ack);
	string_to_ushort(srwnd, rwnd);
	string_to_ushort(sflag, flag);

	return;
}