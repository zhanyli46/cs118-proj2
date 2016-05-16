#ifndef _HELPER_H_
#define _HELPER_H_

#include "util.h"

int handshake_client(uint16_t *seq, uint16_t *ack, uint16_t rwnd);
int handshake_server();
void fill_header(unsigned char *p, uint16_t *seq, uint16_t *ack, uint16_t *rwnd, uint16_t *flag);

#endif