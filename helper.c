#include <string.h>
#include "helper.h"
#include "util.h"

#include <stdio.h>

int handshake_client(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other)
{	
	unsigned char packet[BUFSIZE];
	uint16_t flag;
	ssize_t outbytes;

	memset(packet, 0, BUFSIZE);
	self->seq = init_seqnum();
	self->ack = 0;
	flag = SYN;
	fill_header(packet, &self->seq, &self->ack, &self->rwnd, &flag);


	if ((outbytes = sendto(hinfo->sockfd, packet, BUFSIZE, 0, (struct sockaddr *)hinfo->addr, hinfo->addrlen)) < 0)
		return 0;

	
	return 0;
}

int handshake_server(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other)
{
	unsigned char packet[BUFSIZE];
	uint16_t flag;
	ssize_t inbytes;
	printf("inhandshakeserverif\n");
	if ((inbytes = recvfrom(hinfo->sockfd, packet, BUFSIZE, 0, (struct sockaddr *)hinfo->addr, &hinfo->addrlen) > 0)) {
		
		int i;
		for (i = 0; i < BUFSIZE; i++) {
			if (i % 8 == 0)
				printf("\n");
			printf("%.2x", packet[i]);
		}
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
	memcpy(seq, p, 1);
	memcpy(seq+1, p+1, 1);
	memcpy(ack, p+2, 1);
	memcpy(ack+1, p+3, 1);
	memcpy(rwnd, p+4, 1);
	memcpy(rwnd+1, p+5, 1);
	memcpy(flag, p+6, 1);
	memcpy(flag+1, p+7, 1);

	return;
}