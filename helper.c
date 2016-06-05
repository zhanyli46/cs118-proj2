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

void magic_send(unsigned char *p, uint32_t *arg1, uint32_t *arg2)
{
	unsigned char uarg1[4], uarg2[4];
	uint_to_string(arg1, uarg1);
	uint_to_string(arg2, uarg2);
	memcpy(p + HEADERSIZE, uarg1, 1);
	memcpy(p + HEADERSIZE + 1, uarg1 + 1, 1);
	memcpy(p + HEADERSIZE + 2, uarg1 + 2, 1);
	memcpy(p + HEADERSIZE + 3, uarg1 + 3, 1);
	memcpy(p + HEADERSIZE + 4, uarg2, 1);
	memcpy(p + HEADERSIZE + 5, uarg2 + 1, 1);
	memcpy(p + HEADERSIZE + 6, uarg2 + 2, 1);
	memcpy(p + HEADERSIZE + 7, uarg2 + 3, 1);
}

void magic_recv(unsigned char *p, uint32_t *arg1, uint32_t *arg2)
{
	unsigned char uarg1[4], uarg2[4];
	memcpy(uarg1, p + HEADERSIZE, 1);
	memcpy(uarg1 + 1, p + HEADERSIZE + 1, 1);
	memcpy(uarg1 + 2, p + HEADERSIZE + 2, 1);
	memcpy(uarg1 + 3, p + HEADERSIZE + 3, 1);
	memcpy(uarg2, p + HEADERSIZE + 4, 1);
	memcpy(uarg2 + 1, p + HEADERSIZE + 5, 1);
	memcpy(uarg2 + 2, p + HEADERSIZE + 6, 1);
	memcpy(uarg2 + 3, p + HEADERSIZE + 7, 1);
	string_to_uint(uarg1, arg1);
	string_to_uint(uarg2, arg2);
}
