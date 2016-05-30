#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ftransfer.h"

#include <stdio.h>

int ftransfer_sender(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	// send flow/congestion control parameters
	uint16_t cwnd = INITCWND;
	uint16_t ssthresh = 65535;
	uint16_t rwnd = other->rwnd;
	uint16_t availpack = 0;
	uint16_t limit = 0;
	uint16_t bytesonwire = 0;

	// file read control parameters
	off_t foffset = 0;
	off_t tempoffset = 0;
	size_t bytesend = 0;
	size_t bytesacked = 0;
	ssize_t bytesread = 0;

	// packet parameters
	unsigned char packet[PACKSIZE];
	uint16_t initseq = self->seq;
	witempool_t pool;
	
	// thread control parameter
	int thrdstop = 0;

	// misc
	pthread_t tid;
	int end = 0;
	int i = 0;

	// create listening thread
	userdata ud;
	ud.hinfo = hinfo;
	ud.self = self;
	ud.other = other;
	ud.thrdstop = &thrdstop;
	ud.cwnd = &cwnd;
	ud.ssthresh = &ssthresh;
	ud.rwnd = &rwnd;
	ud.pool = &pool;

	if (pthread_create(&tid, NULL, listen_packet, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}

	pool.list = NULL;
	pool.nitems = 0;
	pool.size = 4;
	pool.list = calloc(pool.size, sizeof(witem_t));

	lseek(filefd, 0, SEEK_SET);
	
	while (!end) {
		// while there's more data and hasn't received all ACKs
		//	1. send data up to the smaller of cwnd and rwnd
		//	2. wait for ACKs

		limit = (cwnd < rwnd) ? cwnd : rwnd;
		availpack = limit / PACKSIZE;
		for (i = 0; i < availpack; i++) {
			if (bytesacked == fsize)
				break;
			memset(packet, 0, PACKSIZE);
			
			// read data
			if ((bytesread = read(filefd, packet + HEADERSIZE, DATASIZE)) < 0) {
				fprintf(stderr, "Error reading file\n");
				return -1;
			}
			// set header
			self->seq = initseq + foffset % MAXSEQNUM;
			self->flag = bytesread << 3;
			// log operation
			add_item(&pool, foffset, self->seq, bytesread);
			foffset += bytesread;
			// send packet
			fprintf(stdout, "Sending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Error sending packet\n");
				return -1;
			}

		}
		break;
	}


	return -1;
}

int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	return -1;
}

static int readdata(int filefd, swnditem_t *item)
{
	ssize_t bytesread;
	if ((bytesread = read(filefd, item->data, DATASIZE)) > 0) {
		item->datalen = bytesread;
		return 1;
	} else if (bytesread == 0) {
		// no more data
		return 0;
	} else {
		fprintf(stderr, "Error: cannot read file\n");
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
				*ud->cwnd = 1;
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
					memcpy(ud->lastpack, &(ud->witems)[i], sizeof(swnditem_t));
					break;
				}
			}
		}
	}
	return NULL;
}

static void *listen_packet(void *ud)
{

	return NULL;
}

static void add_item(witempool_t *pool, unsigned offset, uint16_t seq, uint16_t datalen)
{
	if (pool->nitems == pool->size) {
		pool->size *= 2;
		pool->list = realloc(pool->list, pool->size);
	}

	(pool->list)[pool->nitems].offset = offset;
	(pool->list)[pool->nitems].seq = seq;
	(pool->list)[pool->nitems].datalen = datalen;
	(pool->list)[pool->nitems].nacked = 0;
	pool->nitems += 1;
}