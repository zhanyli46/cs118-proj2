#ifndef _HELPER_H_
#define _HELPER_H_


int handshake_client(unsigned short *seq, unsigned short *ack, unsigned short rwnd);
int handshake_server();

#endif