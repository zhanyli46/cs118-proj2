#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ftransfer.h"
#include <stdio.h>

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other)
{
	// set send window corresponding to the receiver's window size
	unsigned swnd = other->rwnd / PACKSIZE;		// receiver window size
	unsigned cwnd = 1;							// congestion window size
	unsigned limit;								// the smaller of swnd and cwnd
	unsigned packind;							// packet index
	wnditem_t *witems;							// elements in a window
	unsigned char packet[PACKSIZE];				// a packet including header and data
	uint16_t nextseq = self->seq;				// sequence number for next packet
	int end = 0;								// termination boolean
	unsigned head = 0, tail;
	int i;

	witems = calloc(swnd, sizeof(wnditem_t));
	for (i = 0; i < swnd; i++) {
		witems[i].seq = 0;
		witems[i].stat = PACK_UNSENT;
	}


	while (!end) {
		limit = (swnd < cwnd) ? swnd : cwnd;
		tail = (head + limit) % swnd;

		for (packind = 0; packind < limit; ) {
			if (witems[packind].stat == PACK_UNSENT) {
				memset(packet, 0, PACKSIZE);
				// set timer and send packet
				if (readdata(filefd, &witems[packind])) {
					witems[packind].seq = nextseq;
					self->seq = nextseq;
					nextseq += witems[packind].datalen;
					self->ack = 0;
					self->flag = witems[packind].datalen << 3;
					memcpy(packet + HEADERSIZE, witems[packind].data, witems[packind].datalen);
					gettimeofday(&witems[packind].tv, NULL);
					if (send_packet(packet, hinfo, self, other) < 0) {
						fprintf(stderr, "Error sending packet\n");
						exit(1);
					}
					witems[packind].stat = PACK_SENT;
				
				} else {

				}
				packind++;
			}
		}
		break;
	}

	return -1;
}

int ftransfer_recver(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other)
{
	return -1;
}

static int readdata(int filefd, wnditem_t *item)
{
	ssize_t bytesread;
	if ((bytesread = read(filefd, item->data, DATASIZE)) > 0) {
		item->datalen = bytesread;
		return 1;
	} else if (bytesread == 0) {
		// no more data
		return 0;
	} else {
		exit(1);
	}
}