#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ftransfer.h"
#include <stdio.h>

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other)
{
	// set send window corresponding to the receiver's window size
	unsigned wsize = other->rwnd / PACKSIZE;	// receiver window size
	unsigned cwnd = 1;							// congestion window size
	unsigned ssthresh = INITSSTHRESH;			// ssthresh value
	unsigned limit;								// the smaller of wsize and cwnd
	wnditem_t *witems;							// elements in a window
	wnditem_t lastpack;
	int nacked;
	unsigned char packet[PACKSIZE];				// a packet including header and data
	uint16_t nextseq = self->seq;				// sequence number for next packet
	int end = 0;								// termination boolean
	int nodata = 0, nsent = 0;
	unsigned head = 0, tail;
	pthread_t tid;
	int thrdstop = 0;
	unsigned i;
	struct timeval now;

	witems = calloc(wsize, sizeof(wnditem_t));
	for (i = 0; i < wsize; i++) {
		witems[i].seq = 0;
		witems[i].stat = PACK_UNSENT;
	}

	userdata_t ud;
	ud.hinfo = hinfo;
	ud.self = self;
	ud.other = other;
	ud.thrdstop = &thrdstop;
	ud.witems = witems;
	ud.cwnd = &cwnd;
	ud.ssthresh = &ssthresh;
	ud.wsize = &wsize;
	ud.head = &head;
	ud.tail = &tail;
	ud.lastpack = &lastpack;
	ud.nacked = &nacked;
	lastpack.seq = 0;
	lastpack.datalen = 0;

	if (pthread_create(&tid, NULL, *recv_ack, &ud) != 0) {
		fprintf(stderr, "Error: cannot create pthread\n");
		exit(1);
	}

	while (!end) {
		limit = (wsize < cwnd) ? wsize : cwnd;
		tail = (head + limit) % wsize;

		// UNSENT (send packet if there's space in the queue and unsent data)
		for (i = head; i != tail; i = (i + 1) % wsize) {
			if (witems[i].stat == PACK_UNSENT) {
				if (nodata == 1)
					break;
				memset(packet, 0, PACKSIZE);
				// set timer and send packet
				if (readdata(filefd, &witems[i])) {
					witems[i].seq = nextseq;
					self->seq = nextseq;
					nextseq =  (nextseq + witems[i].datalen) % wsize;
					self->ack = 0;
					self->flag = witems[i].datalen << 3;
					memcpy(packet + HEADERSIZE, witems[i].data, witems[i].datalen);
					fprintf(stdout, "Sending data packet %hu %u %u\n", 
						self->seq, cwnd * PACKSIZE, ssthresh * PACKSIZE);
					gettimeofday(&witems[i].tv, NULL);
					if (send_packet(packet, hinfo, self, other) < 0) {
						fprintf(stderr, "Error sending packets\n");
						exit(1);
					}
					nsent++;
					witems[i].stat = PACK_SENT;
				} else {
					nodata = 1;
				}
			}
		}

		// SENT (check timer for each sent packet for retransmission)
		for (i = head; i != tail; i = (i + 1) % wsize) {
			gettimeofday(&now, NULL);
			long deltausec = now.tv_usec - witems[i].tv.tv_usec;
			int deltasec = now.tv_sec - witems[i].tv.tv_sec;
			if (deltasec > 0 || deltausec > TIMEOUT * 1000) {
				self->seq = witems[i].seq;
				self->ack = 0;
				self->flag = witems[i].datalen << 3;
				memcpy(packet + HEADERSIZE, witems[i].data, witems[i].datalen);
				fprintf(stdout, "Sending data packet %hu %u %u Retransmission\n", 
						self->seq, cwnd * PACKSIZE, ssthresh * PACKSIZE);
				gettimeofday(&witems[i].tv, NULL);
				if (send_packet(packet, hinfo, self, other) < 0) {
					fprintf(stderr, "Error sending packets\n");
					exit(1);
				}
			}
		}

		// ACKED (move window frame)
		for (i = head; i != tail; i = (i + 1) % wsize) {
			if (witems[i].stat == PACK_ACKED) {
				memset(&(witems[i].tv), 0, sizeof(struct timeval));
				memset(witems[i].data, 0, DATASIZE);
				witems[i].stat = PACK_UNSENT;
				head = (head + 1) % wsize;
			}
		}

		//  check for duplicate ACKs and retransmission
		if (nacked >= 3) {
			self->seq = lastpack.seq;
			self->ack = 0;
			self->flag = lastpack.datalen << 3;
			memcpy(packet + HEADERSIZE, lastpack.data, lastpack.datalen);
			fprintf(stdout, "Sending data packet %hu %u %u Retransmission\n", 
						self->seq, cwnd * PACKSIZE, ssthresh * PACKSIZE);
			gettimeofday(&lastpack.tv, NULL);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Error sending packets\n");
				exit(1);
			}
		}

		end = nodata && (nsent == 0);
		break;
	}

	return 0;
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

static void *recv_ack(void *userdata)
{
	userdata_t *ud = (userdata_t *)userdata;
	uint16_t ack = 0;
	unsigned i, j;

	unsigned char packet[PACKSIZE];
	while (!*ud->thrdstop) {
		memset(packet, 0, PACKSIZE);
		if (recv_packet(packet, ud->hinfo, ud->self, ud->other) < 0) {
			fprintf(stderr, "Error receiving ack packets\n");
			exit(2);
		} else {
			// if received duplicated ACK
			if (ud->other->ack == ud->lastpack->seq + ud->lastpack->datalen) {
				fprintf(stdout, "Receiving ACK packet %hu\n", ud->other->ack);
				*ud->nacked += 1;
				*ud->wsize = ud->other->rwnd / PACKSIZE;
				if (*ud->cwnd < *ud->ssthresh) {
					*ud->cwnd *= 2;
				} else {
					*ud->cwnd += 1;
				}
				if (*ud->cwnd > *ud->wsize)
					*ud->cwnd = *ud->wsize;
				continue;
			}
			// if received different ACK
			for (i = *ud->head; i != *ud->tail; i = (i + 1) % *ud->wsize) {
				fprintf(stdout, "Receiving ACK packet %hu\n", ud->other->ack);
				if (ud->other->ack == (ud->witems)[i].seq + (ud->witems[i].datalen)) {
					// all packets before this ACK have been received
					for (j = *ud->head; j != i; j = (j + 1) % *ud->wsize)
						(ud->witems)[j].stat = PACK_ACKED;
					*ud->wsize = ud->other->rwnd / PACKSIZE;
					if (*ud->cwnd < *ud->ssthresh) {
						*ud->cwnd *= 2;
					} else {
						*ud->cwnd += 1;
					}
					if (*ud->cwnd > *ud->wsize)
						*ud->cwnd = *ud->wsize;
					// mark this ACK as the last received packet
					memcpy(ud->lastpack, &(ud->witems)[i], sizeof(wnditem_t));
					break;
				}
			}
		}
	}
	return NULL;
}