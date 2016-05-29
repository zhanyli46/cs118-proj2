#include <string.h>
#include "helper.h"
#include "util.h"



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

ssize_t send_packet(unsigned char* packet, hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	ssize_t	outbytes;
	fill_header(packet, &self->seq, &self->ack, &self->rwnd, &self->flag);

	outbytes = sendto(hinfo->sockfd, packet, PACKSIZE, 0, (struct sockaddr *)hinfo->addr, hinfo->addrlen);
	return outbytes;
}

ssize_t recv_packet(unsigned char* packet, hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	ssize_t inbytes;
	if ((inbytes = recv(hinfo->sockfd, packet, PACKSIZE, 0)) >= 0) {
		interpret_header(packet, &other->seq, &other->ack, &other->rwnd, &other->flag);
	}

	return inbytes;
}
