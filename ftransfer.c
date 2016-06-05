#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include "ftransfer.h"


pthread_mutex_t wmutex;
pthread_mutex_t bmutex;

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
	ssize_t bytesend = 0;
	ssize_t bsend = 0;
	ssize_t bytesacked = 0;
	ssize_t bytesread = 0;
	int nodata = 0;

	// packet parameters
	unsigned char packet[PACKSIZE];
	uint16_t initseq = self->seq;
	wnditempool_t witems;

	// retransmission parameters
	struct timeval curtime;
	time_t d_sec = 0;
	long int d_usec = 0;
	uint16_t acknum = 0;
	int nacked = 0;
	ssize_t resendlen = 0;
	int temphead = 0;
	int temptail = 0;
	int tempnitems = 0;
	
	// thread sync control parameter
	pthread_t tid;
	int thrdstop = 0;

	// misc
	int end = 0;
	int i = 0;


	// create listening thread
	sendudata_t ud;
	ud.hinfo = hinfo;
	ud.self = self;
	ud.other = other;
	ud.thrdstop = &thrdstop;
	ud.cwnd = &cwnd;
	ud.ssthresh = &ssthresh;
	ud.rwnd = &rwnd;
	ud.nacked = &nacked;
	ud.acknum = &acknum;
	ud.bytesend = &bytesend;
	ud.witems = &witems;

	if (pthread_create(&tid, NULL, listen_ackpacket, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}
	if (pthread_mutex_init(&wmutex, NULL) != 0) {
		fprintf(stderr, "Error: cannot create thread wmutex\n");
		return -1;
	}

	witems.head = 0;
	witems.tail = 0;
	witems.nitems = 0;
	witems.size = MAXSEQNUM / DATASIZE;
	witems.list = calloc(witems.size * 2, sizeof(wnditem_t));

	lseek(filefd, 0, SEEK_SET);
	
	while (!end) {
		// assume no packet loss, no congestion window
		cwnd = 1*1032;
		limit = (cwnd < rwnd) ? cwnd : rwnd;
		availpack = (limit - bytesend) / PACKSIZE;

		if (foffset != fsize) {
			for (i = 0; i < availpack; i++) {
				memset(packet, 0, PACKSIZE);
				// read data
				lseek(filefd, foffset, SEEK_SET);
				if ((bytesread = read(filefd, packet + HEADERSIZE, DATASIZE)) < 0) {
					fprintf(stderr, "Error reading file\n");
					return -1;
				} else if (bytesread == 0) {
					goto CHECK;
				}
				fprintf(stderr, "Reading at offset %zd for %zd bytes\n", foffset, bytesread);

				// set header
				self->seq = (initseq + foffset) % MAXSEQNUM;
				self->flag = bytesread << 3;
				// log operation as wnditem_t
				gettimeofday(&curtime, NULL);

				while (1) {
					if (witems.nitems < witems.size) {
						pthread_mutex_lock(&wmutex);
						add_witem(&witems, foffset, self->seq, bytesread, &curtime);
						pthread_mutex_unlock(&wmutex);
						break;
					}
				}
				foffset += bytesread;

				// send packet
				fprintf(stderr, "\tSending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
				if ((bsend = send_packet(packet, hinfo, self, other)) < 0) {
					fprintf(stderr, "Error sending packet\n");
					return -1;
				}
				bytesend += bsend;
			}
		}

		pthread_mutex_lock(&wmutex);
		RECHECK_TIMEOUT:
		if (witems.nitems != 0) { 
			for (i = witems.head; i != witems.tail; i = (i + 1) % witems.size) {
				if ((witems.list[i].seq < acknum) && (acknum - witems.list[i].seq < OFTHRESH)) {
					remove_witem(&witems, i);
					goto RECHECK_TIMEOUT;
				}
				gettimeofday(&curtime, NULL);
				d_sec = curtime.tv_sec - witems.list[i].tv.tv_sec;
				d_usec = curtime.tv_usec - witems.list[i].tv.tv_usec;

				if ((d_sec * SECTOUSEC + d_usec) > (TIMEOUT * MSECTOUSEC)) {
					// timeout, retransmit segment
					memset(packet, 0, PACKSIZE);
					gettimeofday(&curtime, NULL);
					d_sec = curtime.tv_sec - witems.list[i].tv.tv_sec;
					d_usec = curtime.tv_usec - witems.list[i].tv.tv_usec;

					// retransmit
					if (d_sec * 1000000 + d_usec > TIMEOUT * 1000) {
						cwnd = INITCWND;
						tempoffset = lseek(filefd, 0, SEEK_CUR);
						lseek(filefd, witems.list[i].offset, SEEK_SET);

						// re-read the data at offset
						if ((bytesread = read(filefd, packet + HEADERSIZE, witems.list[i].datalen)) < 0) {
							fprintf(stderr, "Error reading file\n");
							return -1;
						}
						// set header
						self->seq = witems.list[i].seq;
						self->flag = witems.list[i].datalen << 3;
						// update logged wnditem_t
						update_timer(&witems, i, &curtime);
						foffset = tempoffset;
						// resend packet
						fprintf(stdout, "\tSending data packet %hu %hu %hu Retransmission timeout\n", self->seq, cwnd, ssthresh);
						if (send_packet(packet, hinfo, self, other) < 0) {
							fprintf(stderr, "Error sending retransmission packet\n");
							return -1;
						}
					}
				}
			}
		}
		pthread_mutex_unlock(&wmutex);

		CHECK:
			end = (foffset == fsize);

	}

	while (1) {
		if (witems.nitems == 0) {
			thrdstop = 1;
			break;
		}
	}

	// wait for listening thread to finish
	pthread_cancel(tid);
	pthread_join(tid, NULL);
	pthread_mutex_destroy(&wmutex);
	return 0;
}

int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	// file write control parameters
	off_t foffset = 0;

	// packet and buffering parameters
	unsigned char packet[PACKSIZE];
	uint16_t initack = other->seq + 1;
	uint16_t nextack = initack;
	uint16_t tempack = 0;
	bufitempool_t bitems;

	// thread control
	pthread_t tid;

	// misc
	int end = 0;
	int i = 0;

	// creating listening thread
	recvudata_t ud;
	ud.hinfo = hinfo;
	ud.self = self;
	ud.other = other;
	ud.bitems = &bitems;
	ud.initack = initack;
	ud.nextack = &nextack;
	ud.foffset = &foffset;
	ud.end = &end;

	if (pthread_create(&tid, NULL, listen_datapacket, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}
	if (pthread_mutex_init(&bmutex, NULL) != 0) {
		fprintf(stderr, "Error: cannot create thread bmutex\n");
		return -1;
	}

	bitems.head = 0;
	bitems.tail = 0;
	bitems.nitems = 0;
	bitems.size = MAXSEQNUM / DATASIZE;
	bitems.list = calloc(bitems.size * 2, sizeof(bufitem_t));

	lseek(filefd, 0, SEEK_SET);

	while (!end) {
		// check list and write data continuously
		if (bitems.nitems == 0)
			continue;
		pthread_mutex_lock(&bmutex);
		if (bitems.nitems != 0) {
			i = bitems.head;
			do {
				if (bitems.list[i].offset != foffset) {
					i = (i + 1) % bitems.size;
					continue;
				}
				// write next chunk of data
				fprintf(stderr, "Write file at offset %zd for %zd bytes\n", foffset, bitems.list[i].datalen);
				lseek(filefd, foffset, SEEK_SET);
				if (write(filefd, bitems.list[i].data, bitems.list[i].datalen) < 0) {
					fprintf(stderr, "Error: cannot write to file\n");
				}
				// update new file offset
				foffset += bitems.list[i].datalen;

				// send ack conditionally
				printf("ack should be %lld, cur %hu\n", (foffset + initack) % MAXSEQNUM, nextack);
				tempack = (foffset + initack) % MAXSEQNUM;
				if ((tempack > nextack) ||
					((tempack < nextack) && (nextack - tempack > OFTHRESH))) {
					nextack = (foffset + initack) % MAXSEQNUM;
					memset(packet, 0, PACKSIZE);
					self->ack = nextack;
					self->flag = (bitems.list[i].datalen << 3) | ACK;
					fprintf(stderr, "\tSending ACK packet %hu\n", nextack);
					if (send_packet(packet, hinfo, self, other) < 0) {
						fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
						exit(2);
					}
				}
				// remove the written item from buffer
				remove_bitem(&bitems, i);
				
				i = (i + 1) % bitems.size;
			} while (i != bitems.tail);
		}
		pthread_mutex_unlock(&bmutex);

		/*
		for (i = bitems.head; i != bitems.tail; i = (i + 1) % bitems.size) {
			if (bitems.list[i].offset != foffset)
				continue;
			// write next chunk of data
			fprintf(stderr, "Write file at offset %zd for %zd bytes\n", foffset, bitems.list[i].datalen);
			lseek(filefd, foffset, SEEK_SET);
			if (write(filefd, bitems.list[i].data, bitems.list[i].datalen) < 0) {
				fprintf(stderr, "Error: cannot write to file\n");
			}
			// update new file offset
			foffset += bitems.list[i].datalen;

			// send ack conditionally
			printf("ack should be %lld, cur %hu\n", (foffset + initack) % MAXSEQNUM, nextack);
			tempack = (foffset + initack) % MAXSEQNUM;
			if ((tempack > nextack) ||
				((tempack < nextack) && (nextack - tempack > OFTHRESH))) {
				nextack = (foffset + initack) % MAXSEQNUM;
				memset(packet, 0, PACKSIZE);
				self->ack = nextack;
				self->flag = (bitems.list[i].datalen << 3) | ACK;
				fprintf(stderr, "Sending ACK packet %hu\n", nextack);
				if (send_packet(packet, hinfo, self, other) < 0) {
					fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
					exit(2);
				}
			}

			// remove the written item from buffer
			remove_bitem(&bitems, i);
		}*/
	}

	pthread_join(tid, NULL);
	pthread_mutex_destroy(&bmutex);
	return 0;
}

static void *listen_ackpacket(void *userdata)
{
	sendudata_t *ud = (sendudata_t *)userdata;
	hostinfo_t *hinfo = ud->hinfo;
	conninfo_t *self = ud->self;
	conninfo_t *other = ud->other;
	int *thrdstop = ud->thrdstop;
	uint16_t *cwnd = ud->cwnd;
	uint16_t *ssthresh = ud->ssthresh;
	uint16_t *rwnd = ud->rwnd;
	uint16_t *acknum = ud->acknum;
	int *nacked = ud->nacked;
	ssize_t *bytesend = ud->bytesend;
	wnditempool_t *witems = ud->witems;
	int i;

	unsigned char packet[PACKSIZE];

	while (!*thrdstop) {
		// listen for packet
		memset(packet, 0, PACKSIZE);
		if (recv_packet(packet, hinfo, self, other) < 0) {
			fprintf(stderr, "Fatal error: cannot receive ACK packets, aborting\n");
			exit(2);
		}
		// discard packets that is not ACK
		if ((other->flag & 0x7) != ACK)
			continue;
		fprintf(stderr, "\tReceiving ACK packet %hu\n", other->ack);
		printf("acknum = %d, received ack = %hu\n", *acknum, other->ack);

		pthread_mutex_lock(&wmutex);
		if (*acknum == other->ack) {
			*nacked += 1;
		} else {
			*acknum = other->ack;
			*nacked = 1;
		}
		if (witems->nitems != 0) {
			i = witems->head;
			do {
				if ((witems->list[i].seq < *acknum) || 
					((witems->list[i].seq > *acknum) && (witems->list[i].seq - *acknum > OFTHRESH))) {
					*bytesend -= witems->list[i].datalen + HEADERSIZE;
					remove_witem(witems, i);
				}
				i = (i + 1) % witems->size;
			} while (i != witems->tail);
		}
		pthread_mutex_unlock(&wmutex);

		// modify congestion window
		/*if (*cwnd < *ssthresh) {
			*cwnd *= 2;
		} else {
			*cwnd += 1;
		}*/

		
	}
	
	pthread_exit(0);
}

static void *listen_datapacket(void *userdata)
{
	// retrieve passed data from userdata
	recvudata_t *ud = (recvudata_t *)userdata;
	hostinfo_t *hinfo = ud->hinfo;
	conninfo_t *self = ud->self;
	conninfo_t *other = ud->other;
	bufitempool_t *bitems = ud->bitems;
	uint16_t initack = ud->initack;
	uint16_t *nextack = ud->nextack;
	off_t *foffset = ud->foffset;
	int *end = ud->end;

	unsigned char packet[PACKSIZE];
	off_t offset = 0;
	uint16_t datalen = 0;
	int multiplier = 0;
	uint16_t lastseq = initack;
	int deltaseq = 0;
	off_t cumuoffset = 0;
	int i;


	while (1) {
		// listen for packet
		CONT:
		memset(packet, 0, PACKSIZE);
		if (recv_packet(packet, hinfo, self, other) < 0) {
			fprintf(stderr, "Fatal error: cannot receive data packets, aborting\n");
			exit(2);
		}
		fprintf(stderr, "\tReceiving data packet %hu\n", other->seq);

		if ((other->flag & 0x7) == FIN) {
			fprintf(stderr, "\tReceiving FIN packet\n");
			*end = 1;
			break;
		}

		deltaseq = (lastseq < other->seq) ? other->seq - lastseq : lastseq - other->seq;
		// check if seq has overflowed
		if ((deltaseq > OFTHRESH) && (other->seq < lastseq)) {
			multiplier += 1;
		}
		offset = (other->seq + multiplier * MAXSEQNUM) - initack;
		printf("offset = %jd\n", offset);
		
		datalen = other->flag >> 3;
		
		// check if the packet data is already received
		if (offset < *foffset) {
			self->ack = *nextack;
			self->flag = (datalen << 3) | ACK;
			memset(packet, 0, PACKSIZE);
			fprintf(stderr, "\tSending ACK packet %hu Retransmission\n", *nextack);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
				exit(2);
			}
			continue;
		}

		lastseq = other->seq;
		
		while (1) {
			if (bitems->nitems < bitems->size && offset >= *foffset) {
				pthread_mutex_lock(&bmutex);
				add_bitem(bitems, offset, other->seq, packet + HEADERSIZE, datalen);
				pthread_mutex_unlock(&bmutex);
				break;
			}
		}
	}
	pthread_exit(0);
}

static void add_witem(wnditempool_t *witems, off_t offset, uint16_t seq, uint16_t datalen, struct timeval *tv)
{
	int i = witems->head;
	if (witems->nitems != 0) {
		do {
			if (witems->list[i].offset == offset)
				return;
			i = (i + 1) % witems->size;
		} while (i != witems->tail);
	}
	(witems->list)[witems->tail].offset = offset;
	(witems->list)[witems->tail].seq = seq;
	(witems->list)[witems->tail].datalen = datalen;
	(witems->list)[witems->tail].tv.tv_sec = tv->tv_sec;
	(witems->list)[witems->tail].tv.tv_usec = tv->tv_usec;
	witems->tail = (witems->tail + 1) % witems->size;
	witems->nitems += 1;
	printf("witem %jd added, head %d tail %d nitems %d\n", offset, witems->head, witems->tail, witems->nitems);
}

static void remove_witem(wnditempool_t *witems, int index)
{
	uint16_t temp = witems->list[index].seq;
	if (index == witems->tail) {
		memset(&witems->list[index], 0, sizeof(wnditem_t));
		if (index == 0) {
			witems->tail = witems->tail + witems->size - 1;
		} else {
			witems->tail -= 1;
		}
		witems->nitems -= 1;
	} else if (index == witems->head) {
		memset(&witems->list[index], 0, sizeof(wnditem_t));
		witems->head = (witems->head + 1) % witems->size;
		witems->nitems -= 1;
	} else if (index > witems->head && index < witems->tail && witems->head < witems->tail) {
		memmove(&witems->list[index], &witems->list[index + 1], witems->tail - index);
		memset(&witems->list[witems->tail], 0, sizeof(wnditem_t));
		witems->tail -= 1;
		witems->nitems -= 1;
	} else if (index > witems->head && index > witems->tail && witems->head > witems->tail) {
		memmove(&witems->list[witems->head + 1], &witems->list[witems->head], index - witems->head);
		memset(&witems->list[witems->head], 0, sizeof(wnditem_t));
		witems->head += 1;
		witems->nitems -= 1;
	} else if (index < witems->tail && index < witems->head && witems->tail < witems->head) {
		memmove(&witems->list[index], &witems->list[index + 1], witems->tail - index);
		memset(&witems->list[witems->tail], 0, sizeof(wnditem_t));
		witems->tail -= 1;
		witems->nitems -= 1;
	} else {
		return;
	}
	printf("witem %hu removed, head %d tail %d nitems %d\n", temp, witems->head, witems->tail, witems->nitems);
}

static void add_bitem(bufitempool_t *bitems, off_t offset, uint16_t seq, unsigned char *data, uint16_t datalen)
{
	int i = bitems->head;
	if (bitems->nitems != 0) {
		do {
			if (bitems->list[i].offset == offset)
				return;
			i = (i + 1) % bitems->size;
		} while (i != bitems->tail);
	}
	(bitems->list)[bitems->tail].offset = offset;
	(bitems->list)[bitems->tail].seq = seq;
	(bitems->list)[bitems->tail].datalen = datalen;
	(bitems->list)[bitems->tail].data = malloc(datalen);
	memcpy((bitems->list)[bitems->tail].data, data, datalen);
	bitems->tail = (bitems->tail + 1) % bitems->size;
	bitems->nitems += 1;
	printf("bitem %jd added, head %d tail %d nitems %d\n", offset, bitems->head, bitems->tail, bitems->nitems);
}

static void remove_bitem(bufitempool_t *bitems, int index)
{
	off_t temp = bitems->list[index].offset;
	if (bitems->head == index) {
		memset(&bitems->list[index], 0, sizeof(bufitem_t));
		bitems->head = (bitems->head + 1) % bitems->size;
		bitems->nitems -= 1;
	} else if (index > bitems->head && index < bitems->tail && bitems->head < bitems->tail) {
		memmove(&bitems->list[index], &bitems->list[index + 1], bitems->tail - index);
		memset(&bitems->list[bitems->tail], 0, sizeof(bufitem_t));
		bitems->tail -= 1;
		bitems->nitems -= 1;
	} else if (index > bitems->head && index > bitems->tail && bitems->head > bitems->tail) {
		memmove(&bitems->list[bitems->head + 1], &bitems->list[bitems->head], index - bitems->head);
		memset(&bitems->list[bitems->head], 0, sizeof(bufitem_t));
		bitems->head += 1;
		bitems->nitems -= 1;
	} else if (index < bitems->tail && index < bitems->head && bitems->tail < bitems->head) {
		memmove(&bitems->list[index], &bitems->list[index + 1], bitems->tail - index);
		memset(&bitems->list[bitems->tail], 0, sizeof(bufitem_t));
		bitems->tail -= 1;
		bitems->nitems -= 1;
	} else {
		return;
	}
	printf("bitem %jd removed, head %d tail %d nitems %d\n", temp, bitems->head, bitems->tail, bitems->nitems);
}

static void update_timer(wnditempool_t *witems, int index, struct timeval *tv)
{
	memcpy(&(witems->list)[index].tv, tv, sizeof(struct timeval));
}