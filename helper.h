#ifndef _HELPER_H_
#define _HELPER_H_

#include "util.h"

typedef struct hostinfo {
	int sockfd;
	struct sockaddr_in *addr;
	socklen_t addrlen;
} hostinfo_t;

typedef struct tcpconst {
	uint16_t seq;
	uint16_t ack;
	uint16_t rwnd;
} tcpconst_t;

int handshake_client(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other);
int handshake_server(hostinfo_t *hinfo, tcpconst_t *self, tcpconst_t *other);
void fill_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag);
void interpret_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag);

#endif