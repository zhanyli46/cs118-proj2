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

	// file read control parameters
	off_t sendoffset = 0;
	off_t recvoffset = 0;
	off_t tempoffset = 0;
	ssize_t bytesonwire = 0;
	ssize_t bsend = 0;
	ssize_t bytesacked = 0;
	ssize_t bytesread = 0;

	// packet parameters
	unsigned char packet[PACKSIZE];
	uint16_t initseq = self->seq;
	uint32_t param1, param2;
	wnditempool_t witems;

	// retransmission parameters
	struct timeval curtime;
	time_t d_sec = 0;
	long int d_usec = 0;
	uint16_t acknum = 0;
	int nacked = 0;
	ssize_t resendlen = 0;
	
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
	ud.bytesonwire = &bytesonwire;
	ud.recvoffset = &recvoffset;
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
	witems.size = MAXSEQNUM / DATASIZE * 2;
	witems.list = calloc(witems.size, sizeof(wnditem_t));

	lseek(filefd, 0, SEEK_SET);
	
	while (!end) {
		// assume no packet loss, no congestion window
		limit = (cwnd < rwnd) ? cwnd : rwnd;
		availpack = (limit - bytesonwire) / PACKSIZE;

		// when there's more data to read
		if (sendoffset != fsize) {
			for (i = 0; i < availpack; i++) {
				memset(packet, 0, PACKSIZE);
				// read data
				lseek(filefd, sendoffset, SEEK_SET);
				if ((bytesread = read(filefd, packet + MAGICSIZE, DATASIZE)) < 0) {
					fprintf(stderr, "Error reading file\n");
					return -1;
				} else if (bytesread == 0) {
					goto CHECK;
				}
				fprintf(stderr, "Reading at offset %zd for %zd bytes\n", sendoffset, bytesread);
				param1 = (uint32_t)sendoffset;
				param2 = 0;
				self->seq = (param1 + initseq) % MAXSEQNUM;

				while (1) {
					if (witems.nitems < witems.size) {
						pthread_mutex_lock(&wmutex);
						gettimeofday(&curtime, NULL);
						add_witem(&witems, sendoffset, param1, bytesread, &curtime);
						pthread_mutex_unlock(&wmutex);
						break;
					}
				}
				magic_send(packet, &param1, &param2);
				sendoffset += bytesread;

				// send packet
				fprintf(stderr, "Sending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
				if ((bsend = send_packet(packet, hinfo, self, other)) < 0) {
					fprintf(stderr, "Error sending packet\n");
					return -1;
				}
				bytesonwire += bsend;

			}
		}

		// check timer of each segment
		if (witems.nitems != 0) {
			i = witems.head;
			do {
				if (witems.list[i].offset < recvoffset) {
					pthread_mutex_lock(&wmutex);
					remove_witem(&witems, i);
					pthread_mutex_unlock(&wmutex);
				} else {
					gettimeofday(&curtime, NULL);
					d_sec = curtime.tv_sec - witems.list[i].tv.tv_sec;
					d_usec = curtime.tv_usec - witems.list[i].tv.tv_usec;

					if ((d_sec * SECTOUSEC + d_usec) > (TIMEOUT * MSECTOUSEC)) {
						memset(packet, 0, PACKSIZE);
						cwnd = INITCWND;
						lseek(filefd, witems.list[i].offset, SEEK_SET);
						// re-read the data at offset
						if ((bytesread = read(filefd, packet + MAGICSIZE, witems.list[i].datalen)) < 0) {
							fprintf(stderr, "Error reading file\n");
							return -1;
						}

						param1 = (uint32_t)witems.list[i].seq;
						param2 = 0;
						self->seq = (param1 + initseq) % MAXSEQNUM;
						magic_send(packet, &param1, &param2);
						lseek(filefd, sendoffset, SEEK_SET);
						gettimeofday(&curtime, NULL);
						update_timer(&witems, i, &curtime);
						// resend packet
						fprintf(stdout, "\tSending data packet %hu %hu %hu Retransmission timeout\n", self->seq, cwnd, ssthresh);
						if (send_packet(packet, hinfo, self, other) < 0) {
							fprintf(stderr, "Error sending retransmission packet\n");
							return -1;
						}
					}
				}
				i = (i + 1) % witems.size;
			} while (i != witems.tail);
		}

		CHECK:
			end = (sendoffset == fsize) && (recvoffset > sendoffset);

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
	uint32_t *param1 = malloc(sizeof(uint32_t));
	uint32_t *param2 = malloc(sizeof(uint32_t));
	off_t *recvoffset = ud->recvoffset;
	ssize_t *bytesonwire = ud->bytesonwire;
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

		fprintf(stderr, "Receiving ACK packet %hu\n", other->ack);

		magic_recv(packet, param1, param2);
		*recvoffset = (*param2 > *recvoffset) ? *param2 : *recvoffset;
		printf("recvoffset = %lld\n", *recvoffset);
		if (witems->nitems != 0) {
			i = witems->head;
			do {
				if (witems->list[i].seq < *recvoffset) {
					pthread_mutex_lock(&wmutex);
					*bytesonwire -= witems->list[i].datalen + MAGICSIZE;
					remove_witem(witems, i);
					pthread_mutex_unlock(&wmutex);
				}
				i = (i + 1) % witems->size;
			} while (i != witems->tail);
		}
	}
	
	pthread_exit(0);
}

int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	// file write control parameters
	off_t recvoffset = 0;
	off_t saveoffset = 0;

	// packet and buffering parameters
	unsigned char packet[PACKSIZE];
	uint16_t initack = other->seq + 1;
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
	ud.initack = &initack;
	ud.recvoffset = &recvoffset;
	ud.bitems = &bitems;
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
	bitems.size = MAXSEQNUM / DATASIZE * 2;
	bitems.list = calloc(bitems.size, sizeof(bufitem_t));

	lseek(filefd, 0, SEEK_SET);
	while (!end) {
		// save buffered data to local file
		if (bitems.nitems == 0)
			continue;

		i = bitems.head;
		do {
			if (saveoffset == bitems.list[i].offset) {
				// write next chunk of data
				fprintf(stderr, "Write file at offset %zd for %zd bytes\n", saveoffset, bitems.list[i].datalen);
				lseek(filefd, saveoffset, SEEK_SET);
				if (write(filefd, bitems.list[i].data, bitems.list[i].datalen) < 0) {
					fprintf(stderr, "Error: cannot write to file\n");
				}
				// update new saveoffset
				saveoffset += bitems.list[i].datalen;

				// remove data from buffer
				pthread_mutex_lock(&bmutex);
				remove_bitem(&bitems, i);
				pthread_mutex_unlock(&bmutex);

			}

			i = (i + 1) % bitems.size;
		} while (i != bitems.tail);
	}

	pthread_join(tid, NULL);
	pthread_mutex_destroy(&bmutex);
	return 0;
}

static void *listen_datapacket(void *userdata)
{
	// retrieve passed data from userdata
	recvudata_t *ud = (recvudata_t *)userdata;
	hostinfo_t *hinfo = ud->hinfo;
	conninfo_t *self = ud->self;
	conninfo_t *other = ud->other;
	off_t *recvoffset = ud->recvoffset;
	uint16_t *initack = ud->initack;
	bufitempool_t *bitems = ud->bitems;
	int *end = ud->end;

	unsigned char packet[PACKSIZE];
	ssize_t inbytes = 0;
	int datalen;
	uint32_t *param1 = malloc(sizeof(uint32_t));
	uint32_t *param2 = malloc(sizeof(uint32_t));
	int i;

	while (1) {
		memset(packet, 0, PACKSIZE);
		if ((inbytes = recv_packet(packet, hinfo, self, other)) < 0) {
			fprintf(stderr, "Fatal error: cannot receive data packets, aborting\n");
			exit(2);
		} else if (inbytes == 0) {
			continue;
		}
		if ((other->flag & 0x7) == FIN) {
			fprintf(stderr, "Receiving FIN packet\n");
			*end = 1;
			break;
		}
		datalen = inbytes - MAGICSIZE;
		magic_recv(packet, param1, param2);

		// discard packet that are received already
		if (*param1 < *recvoffset)
			continue;

		fprintf(stderr, "Receiving data packet %hu\n", other->seq);
		
		// if packet received is the next packet, send ACK
		if (*param1 == *recvoffset) {
			// add to buffer
			while (1) {
				if (bitems->nitems != bitems->size) {
					pthread_mutex_lock(&bmutex);
					add_bitem(bitems, (off_t)*param1, other->seq, packet + MAGICSIZE, datalen);
					pthread_mutex_unlock(&bmutex);
					break;
				}
			}
			*recvoffset += datalen;
			// send ACK
			memset(packet, 0, PACKSIZE);
			*param1 = 0;
			*param2 = *recvoffset;
			magic_send(packet, param1, param2);
			self->ack = (*recvoffset + *initack) % MAXSEQNUM;
			self->flag = ACK;
			fprintf(stderr, "Sending ACK packet %hu\n", self->ack);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
				exit(2);
			}
			continue;
		}

		if (*param1 > *recvoffset) {
			// add to buffer
			while (1) {
				if (bitems->nitems != bitems->size) {
					pthread_mutex_lock(&bmutex);
					add_bitem(bitems, (off_t)*param1, other->seq, packet + MAGICSIZE, datalen);
					pthread_mutex_unlock(&bmutex);
					break;
				}
			}

			// send duplicate ACK
			memset(packet, 0, PACKSIZE);
			*param1 = 0;
			*param2 = *recvoffset;
			magic_send(packet, param1, param2);
			self->ack = *recvoffset % MAXSEQNUM;
			self->flag = ACK;
			fprintf(stderr, "Sending ACK packet %hu Retrasmission\n", self->ack);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
				exit(2);
			}
			continue;

		}


	}

	return NULL;
}

static void add_witem(wnditempool_t *witems, off_t offset, uint32_t seq, uint16_t datalen, struct timeval *tv)
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
	printf("witem %u added, head %d tail %d nitems %d\n", seq, witems->head, witems->tail, witems->nitems);
}

static void remove_witem(wnditempool_t *witems, int index)
{
	if (witems->nitems == 0)
		return;
	uint32_t temp = witems->list[index].seq;
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
	printf("witem %u removed, head %d tail %d nitems %d\n", temp, witems->head, witems->tail, witems->nitems);
}

static void add_bitem(bufitempool_t *bitems, off_t offset, uint32_t seq, unsigned char *data, uint16_t datalen)
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
	printf("bitem %u added, head %d tail %d nitems %d\n", seq, bitems->head, bitems->tail, bitems->nitems);
}

static void remove_bitem(bufitempool_t *bitems, int index)
{
	uint32_t temp = bitems->list[index].seq;
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
	printf("bitem %u removed, head %d tail %d nitems %d\n", temp, bitems->head, bitems->tail, bitems->nitems);
}

static void update_timer(wnditempool_t *witems, int index, struct timeval *tv)
{
	memcpy(&(witems->list)[index].tv, tv, sizeof(struct timeval));
}