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

	// retransmission parameters
	struct timeval tv;
	time_t d_sec;
	long int d_usec;
	
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

		// send new data
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
			// log operation as witem_t
			gettimeofday(&tv, NULL);
			add_item(&pool, foffset, self->seq, bytesread, &tv);
			foffset += bytesread;
			// send packet
			fprintf(stdout, "Sending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Error sending packet\n");
				return -1;
			}
		}

		// check timer and retransmit old data
		for (i = 0; i < pool.nitems; i++) {
			memset(packet, 0, PACKSIZE);
			gettimeofday(&tv, NULL);
			d_sec = tv.tv_sec - pool.list[i].tv.tv_sec;
			d_usec = tv.tv_usec - pool.list[i].tv.tv_usec;

			// retransmit
			if (d_sec > 1 || d_usec < TIMEOUT * 1000) {
				tempoffset = lseek(filefd, 0, SEEK_CUR);
				lseek(filefd, pool.list[i].offset, SEEK_SET);

				// re-read the data at offset
				if ((bytesread = read(filefd, packet + HEADERSIZE, pool.list[i].datalen)) < 0) {
					fprintf(stderr, "Error reading file\n");
					return -1;
				}
				// set header
				self->seq = pool.list[i].seq;
				self->flag = pool.list[i].datalen;
				// update logged witem_t
				gettimeofday(&tv, NULL);
				update_timer(&pool, i, &tv);

				foffset = tempoffset;
				// resend packet
				fprintf(stdout, "Sending data packet %hu %hu %hu\n Retransmission", self->seq, cwnd, ssthresh);
				if (send_packet(packet, hinfo, self, other) < 0) {
					fprintf(stderr, "Error during retransmission\n");
					return -1;
				}
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

static void *listen_packet(void *ud)
{

	return NULL;
}

static void add_item(witempool_t *pool, unsigned offset, uint16_t seq, uint16_t datalen, struct timeval *tv)
{
	if (pool->nitems == pool->size) {
		pool->size *= 2;
		pool->list = realloc(pool->list, pool->size);
	}

	(pool->list)[pool->nitems].offset = offset;
	(pool->list)[pool->nitems].seq = seq;
	(pool->list)[pool->nitems].datalen = datalen;
	(pool->list)[pool->nitems].nacked = 0;
	memcpy(&(pool->list)[pool->nitems].tv, tv, sizeof(struct timeval));
	pool->nitems += 1;
}

static void update_timer(witempool_t *pool, int index, struct timeval *tv)
{
	memcpy(&(pool->list)[index].tv, tv, sizeof(struct timeval));
}