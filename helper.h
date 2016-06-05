#ifndef _HELPER_H_
#define _HELPER_H_

#include "util.h"

typedef struct {
	int sockfd;
	struct sockaddr_in *addr;
	socklen_t addrlen;
} hostinfo_t;

typedef struct {
	uint16_t seq;
	uint16_t ack;
	uint16_t rwnd;
	uint16_t flag;
} conninfo_t;


void fill_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag);
void interpret_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag);
ssize_t send_packet(unsigned char* packet, hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other, ssize_t datalen);
ssize_t recv_packet(unsigned char* packet, hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other);
void magic_send(unsigned char *p, uint32_t *arg1, uint32_t *arg2);
void magic_recv(unsigned char *p, uint32_t *arg1, uint32_t *arg2);


#endif