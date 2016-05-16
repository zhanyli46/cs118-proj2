#include "helper.h"
#include "util.h"
#include <stdio.h>

int handshake_client(unsigned short *seq, unsigned short *ack, unsigned short rwnd)
{	
	packet init;
	
	*seq = init_seqnum();
	*ack = 0;


	return 0;
}