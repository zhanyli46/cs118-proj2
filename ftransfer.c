#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include "ftransfer.h"

typedef int pthread_ctrl_t;

pthread_mutex_t wmutex;
pthread_mutex_t bmutex;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	// send flow/congestion control parameters
	uint32_t cwnd = INITCWND;
	uint32_t ssthresh = 65535;
	uint16_t rwnd = other->rwnd;
	uint16_t availpack = 0;
	uint16_t limit = 0;

	// file read control parameters
	off_t sendoffset = 0;
	off_t recvoffset = 0;
	off_t faroffset = 0;
	off_t tempoffset = -1;
	ssize_t bytesonwire = 0;
	ssize_t bsend = 0;
	ssize_t bytesacked = 0;
	ssize_t bytesread = 0;

	// packet parameters
	unsigned char packet[PACKSIZE];
	unsigned char *lastpack;
	uint16_t initseq = self->seq;
	uint32_t param1, param2;
	wnditempool_t witems;

	// retransmission parameters
	struct timeval curtime;
	struct timeval timeout;
	time_t d_sec = 0;
	long int d_usec = 0;
	uint16_t acknum = 0;
	int nacked = 0;
	resendstat_t resendstat = T_RESENDFIN;
	
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
	ud.filefd = filefd;
	ud.thrdstop = &thrdstop;
	ud.cwnd = &cwnd;
	ud.ssthresh = &ssthresh;
	ud.rwnd = &rwnd;
	ud.bytesonwire = &bytesonwire;
	ud.sendoffset = &sendoffset;
	ud.recvoffset = &recvoffset;
	ud.resendstat = &resendstat;
	ud.witems = &witems;

	witems.head = 0;
	witems.tail = 0;
	witems.nitems = 0;
	witems.size = MAXSEQNUM / DATASIZE * 2;
	witems.list = calloc(witems.size, sizeof(wnditem_t));

	if (pthread_create(&tid, NULL, listen_ackpacket, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}
	if (pthread_mutex_init(&wmutex, NULL) != 0) {
		fprintf(stderr, "Error: cannot create thread wmutex\n");
		return -1;
	}

	lseek(filefd, 0, SEEK_SET);
	
	while (!end) {
		RESEND:
		// prepare to resend
		pthread_mutex_lock(&wmutex);
		if (resendstat == T_RESENDREQ) {
			printf("other thread demands a resend\n");
			resendstat = T_RESENDBEG;
		}
		pthread_mutex_unlock(&wmutex);

		pthread_mutex_lock(&wmutex);
		if (resendstat != T_RESENDFIN) {
			pthread_mutex_unlock(&wmutex);
			continue;
		}
		pthread_mutex_unlock(&wmutex);

		// assume no packet loss, no congestion window
		//cwnd = PACKSIZE * 5;
		limit = (cwnd < rwnd) ? cwnd : rwnd;
		availpack = (limit - bytesonwire) / PACKSIZE;


		// when there's more data to read
		for (i = 0; i < availpack; i++) {
			if (sendoffset == fsize) {
				break;
			}
			memset(packet, 0, PACKSIZE);
			// read data
			pthread_mutex_lock(&wmutex);
			lseek(filefd, sendoffset, SEEK_SET);
			if (resendstat == T_RESENDREQ) {
				pthread_mutex_unlock(&wmutex);
				goto RESEND;
			}
			if ((bytesread = read(filefd, packet + MAGICSIZE, DATASIZE)) < 0) {
					fprintf(stderr, "Error reading file\n");
				return -1;
			} else if (bytesread == 0) {
				break;
			}
			faroffset = (faroffset < sendoffset) ? sendoffset : faroffset;
			fprintf(stderr, "Reading at offset %zd for %zd bytes\n", sendoffset, bytesread);
			param1 = (uint32_t)sendoffset;
			param2 = 0;
			self->seq = (param1 + initseq) % MAXSEQNUM;
			
			while (1) {
				/*if (resendstat == T_RESENDREQ) {
					pthread_mutex_unlock(&witems);
					goto RESEND;
				}*/
				if (witems.nitems < witems.size) {
					gettimeofday(&curtime, NULL);
					add_witem(&witems, sendoffset, self->seq, bytesread, &curtime);
					break;
				}
			}
			pthread_mutex_unlock(&wmutex);
			
			// send packet
			//fprintf(stderr, "Sending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
			if (sendoffset < faroffset)
				fprintf(stderr, "Sending data packet %lld %u %u Retransmission\n", param1, cwnd, ssthresh);
			else
				fprintf(stderr, "Sending data packet %lld %u %u\n", param1, cwnd, ssthresh);

			magic_send(packet, &param1, &param2);
			if ((bsend = send_packet(packet, hinfo, self, other, bytesread + MAGICSIZE)) < 0) {
				fprintf(stderr, "Error sending packet\n");
				return -1;
			}
			
			sendoffset += bytesread;
			bytesonwire += bsend;
		}
		
		UPDATE:
		// check timer of each segment
		for (i = witems.head; i != witems.tail; i = (i + 1) % witems.size) {
			pthread_mutex_lock(&wmutex);
			gettimeofday(&timeout, NULL);
			d_sec = timeout.tv_sec - witems.list[i].tv.tv_sec;
			d_usec = timeout.tv_usec - witems.list[i].tv.tv_usec;

			if ((d_sec * SECTOUSEC + d_usec) > (TIMEOUT * MSECTOUSEC)) {
				memset(packet, 0, PACKSIZE);
				lseek(filefd, witems.list[i].offset, SEEK_SET);
				// re-read the data at offset
				if ((bytesread = read(filefd, packet + MAGICSIZE, witems.list[i].datalen)) < 0) {
					fprintf(stderr, "Error reading file\n");
					return -1;
				}
				param1 = (uint32_t)witems.list[i].offset;
				param2 = 0;
				self->seq = (param1 + initseq) % MAXSEQNUM;
				magic_send(packet, &param1, &param2);
				lseek(filefd, sendoffset, SEEK_SET);
				// resend packet
				if (witems.list[i].offset < recvoffset) {
					bytesonwire -= witems.list[i].datalen + MAGICSIZE;
					remove_witem(&witems, i);
					pthread_mutex_unlock(&wmutex);
					goto UPDATE;
				} 
				ssthresh = cwnd / 2;
				ssthresh = (ssthresh < 2 * INITCWND) ? 2 * INITCWND : ssthresh;
				cwnd = INITCWND;
				gettimeofday(&witems.list[i].tv, NULL);
				//fprintf(stdout, "Sending data packet %hu %hu %hu Retransmission timeout offset %lld\n",
				//	self->seq, cwnd, ssthresh, witems.list[i].offset);
				fprintf(stdout, "Sending data packet %lld %u %u Retransmission timeout offset %lld\n",
					param1, cwnd, ssthresh, witems.list[i].offset);
				if (send_packet(packet, hinfo, self, other, witems.list[i].datalen + MAGICSIZE) < 0) {
					fprintf(stderr, "Error sending retransmission packet\n");
					return -1;
				}
			}
			pthread_mutex_unlock(&wmutex);
		}

		end = (sendoffset == fsize) && (recvoffset >= sendoffset);
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
	int filefd = ud->filefd;
	int *thrdstop = ud->thrdstop;
	uint32_t *cwnd = ud->cwnd;
	uint32_t *ssthresh = ud->ssthresh;
	uint16_t *rwnd = ud->rwnd;
	off_t *sendoffset = ud->sendoffset;
	off_t *recvoffset = ud->recvoffset;
	ssize_t *bytesonwire = ud->bytesonwire;
	int *resendstat = ud->resendstat;
	wnditempool_t *witems = ud->witems;
	int i;

	unsigned char packet[PACKSIZE];
	uint32_t *param1 = malloc(sizeof(uint32_t));
	uint32_t *param2 = malloc(sizeof(uint32_t));
	uint32_t lastack = 0;
	int nacked = 0;
	ssize_t bytesread;

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

		magic_recv(packet, param1, param2);

		//fprintf(stderr, "Receiving ACK packet %hu\n", other->ack);
		fprintf(stderr, "Receiving ACK packet %lld\n", *param2);

		// count duplicated ACK
		if (lastack != *param2) {
			lastack = *param2;
			nacked = 1;
		} else {
			nacked += 1;
		}

		// trigger duplicate ACK retransmission
		fprintf(stderr, "ACK log: last received ack = %u, nacked = %d\n", lastack, nacked);
		if (nacked >= 3) {
			pthread_mutex_lock(&wmutex);
			printf("resendstat = %d\n", *resendstat);
			*resendstat = T_RESENDREQ;
			printf("update resendstat = %d\n", *resendstat);
			pthread_mutex_unlock(&wmutex);

			// wait for T_RESENDBEG to actually send the data
			while (1) {
				pthread_mutex_lock(&wmutex);
				if (*resendstat == T_RESENDBEG) {
					printf("actual resend\n");
					// remove all witems from current buffer
					while (witems->nitems > 0) {
						remove_witem(witems, witems->head);
					}

					printf("nitems = %d, head %d tail %d\n", witems->nitems, witems->head, witems->tail);
					*bytesonwire = 0;
					// retransmit from the offset indicated by ACK
					printf("reset recvoffset to %u\n", lastack);
					*sendoffset = lastack;

					*resendstat = T_RESENDFIN;
					pthread_mutex_unlock(&wmutex);
					break;
				}
				pthread_mutex_unlock(&wmutex);
			}
			*ssthresh = *cwnd / 2;
			*ssthresh = (*ssthresh < 2 * INITCWND) ? 2 * INITCWND : *ssthresh;
			*cwnd = INITCWND;
			nacked = 1;
			continue;
		}

		// speed up transmission
		if (*cwnd < *ssthresh)
			*cwnd *= 2;
		else
			*cwnd += INITCWND;


		*recvoffset = (*param2 > *recvoffset) ? *param2 : *recvoffset;
		printf("recvoffset = %lld\n", *recvoffset);
		if (witems->nitems != 0) {
			i = witems->head;
			do {
				if (witems->list[i].offset < *recvoffset) {
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
	off_t bufoffset = 0;

	// packet and buffering parameters
	unsigned char packet[PACKSIZE];
	uint16_t initack = other->seq + 1;
	bufitempool_t bitems;

	// thread control
	pthread_t tid;
	int eof = 0;

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
	ud.bufoffset = &bufoffset;
	ud.bitems = &bitems;
	ud.eof = &eof;

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
		pthread_mutex_lock(&bmutex);
		for (i = bitems.head; i != bitems.tail; i = (i + 1) % bitems.size) {
			if (bitems.list[i].offset == bufoffset) {
				// write next chunk of data
				fprintf(stderr, "Write file at offset %zd for %zd bytes\n", bufoffset, bitems.list[i].datalen);
				if (write(filefd, bitems.list[i].data, bitems.list[i].datalen) < 0) {
					fprintf(stderr, "Error: cannot write to file\n");
				}
				// update new bufoffset
				bufoffset += bitems.list[i].datalen;

				// remove data from buffer
				remove_bitem(&bitems, i);
			} else if (bitems.list[i].offset < bufoffset) {
				// remove data from buffer
				remove_bitem(&bitems, i);
			}
		}
		pthread_mutex_unlock(&bmutex);
		end = eof && (bufoffset == recvoffset);
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
	off_t *bufoffset = ud->bufoffset;
	uint16_t *initack = ud->initack;
	bufitempool_t *bitems = ud->bitems;
	int *eof = ud->eof;

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
			*eof = 1;
			break;
		}

		datalen = inbytes - MAGICSIZE;
		magic_recv(packet, param1, param2);

		//fprintf(stderr, "Receiving data packet %hu\n", other->seq);
		fprintf(stderr, "Receiving data packet %lld\n", *param1);

		printf("expected offset %lld, received offset %lld\n", *recvoffset, *param1);
		
		// if packet received is the next packet, send ACK
		if (*param1 == *recvoffset) {
			// add to buffer
			while (1) {
				pthread_mutex_lock(&bmutex);
				if (bitems->nitems < bitems->size) {
					add_bitem(bitems, (off_t)*param1, other->seq, packet + MAGICSIZE, datalen);
					pthread_mutex_unlock(&bmutex);
					break;
				}
				pthread_mutex_unlock(&bmutex);
			}
			*recvoffset += datalen;

			// update recvoffset using buffered data
			pthread_mutex_lock(&bmutex);
			if (bitems->nitems != 0) {
				i = bitems->head;
				do {
					if (bitems->list[i].offset == *recvoffset)
						*recvoffset += bitems->list[i].datalen;
					i = (i + 1) % bitems->size;
				} while (i != bitems->tail);
			}
			pthread_mutex_unlock(&bmutex);
			// send ACK
			memset(packet, 0, PACKSIZE);
			*param1 = 0;
			*param2 = *recvoffset;
			magic_send(packet, param1, param2);
			self->ack = (*recvoffset + *initack) % MAXSEQNUM;
			self->flag = ACK;
			
			//fprintf(stderr, "Sending ACK packet %hu\n", self->ack);
			fprintf(stderr, "Sending ACK packet %lld\n", *param2);

			if (send_packet(packet, hinfo, self, other, PACKSIZE) < 0) {
				fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
				exit(2);
			}
			goto CONT;
		} else if ((*param1 > *recvoffset) && (*param1 > *recvoffset + BUFTHRESH)) {
			// add to buffer
			while (1) {
				pthread_mutex_lock(&bmutex);
				if (bitems->nitems < bitems->size) {
					add_bitem(bitems, (off_t)*param1, other->seq, packet + MAGICSIZE, datalen);
					pthread_mutex_unlock(&bmutex);
					break;
				}
				pthread_mutex_unlock(&bmutex);
			}

		}

		// send duplicate ACK
		memset(packet, 0, PACKSIZE);
		*param1 = 0;
		*param2 = *recvoffset;
		magic_send(packet, param1, param2);
		self->ack = (*recvoffset + *initack) % MAXSEQNUM;
		self->flag = ACK;
		//fprintf(stderr, "Sending ACK packet %hu Retransmission\n", self->ack);
		fprintf(stderr, "Sending ACK packet %lld\n", *param2);
		if (send_packet(packet, hinfo, self, other, PACKSIZE) < 0) {
			fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
			exit(2);
		}


		CONT:
			continue;
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
	witems->list[(witems->tail)].offset = offset;
	witems->list[(witems->tail)].seq = seq;
	witems->list[(witems->tail)].datalen = datalen;
	memcpy(&witems->list[(witems->tail)].tv, tv, sizeof(struct timeval));
	witems->tail = (witems->tail + 1) % witems->size;
	witems->nitems += 1;
}

static void remove_witem(wnditempool_t *witems, int index)
{
	uint32_t temp = witems->list[index].offset;

	if (witems->nitems == 0)
		return;
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
}

static void add_bitem(bufitempool_t *bitems, off_t offset, uint32_t seq, unsigned char *data, uint16_t datalen)
{
	int temp = bitems->tail;

 
	int i = bitems->head;
	if (bitems->nitems != 0) {
		do {
			if (bitems->list[i].offset == offset)
				return;
			i = (i + 1) % bitems->size;
		} while (i != bitems->tail);
	}
	(bitems->list)[(bitems->tail)].offset = offset;
	(bitems->list)[(bitems->tail)].seq = seq;
	(bitems->list)[(bitems->tail)].datalen = datalen;
	(bitems->list)[(bitems->tail)].data = malloc(datalen);
	memcpy((bitems->list)[(bitems->tail)].data, data, datalen);
	bitems->tail = (bitems->tail + 1) % bitems->size;
	bitems->nitems += 1;

	printf("###### adding at %lld , head %d tail %d [%d]\n", offset, bitems->head, bitems->tail, temp);
}

static void remove_bitem(bufitempool_t *bitems, int index)
{
	off_t temp = bitems->list[index].offset;

	if (bitems->nitems == 0)
		return;

	if (bitems->tail == index) {
		memset(&bitems->list[0], 0, sizeof(bufitem_t));
		if (bitems->tail == 0) {
			bitems->tail = bitems->size - 1;
		} else
			bitems->tail -= 1;
		bitems->nitems -= 1;
	} else if (bitems->head == index) {
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
	printf("$$$$$$ removing from %lld, head %d tail %d [%d]\n", temp, bitems->head, bitems->tail, index);
}