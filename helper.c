#include <string.h>
#include "helper.h"
#include "util.h"
#include <stdio.h>

int handshake_client(uint16_t *seq, uint16_t *ack, uint16_t rwnd)
{	
	unsigned char packet[BUFSIZE];
	memset(packet, 0, BUFSIZE);
	uint16_t flag;
	*seq = init_seqnum();
	*ack = 0;
	flag = SYN;
	fill_header(packet, seq, ack, &rwnd, &flag);

	
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