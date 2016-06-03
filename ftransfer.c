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
	
	// thread control parameter
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

printf("%s\n", "Creating thread");
	if (pthread_create(&tid, NULL, listen_ackpacket, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}

	witems.nitems = 0;
	witems.size = 4;
	witems.list = calloc(witems.size, sizeof(wnditem_t));

	lseek(filefd, 0, SEEK_SET);
	
	while (!end) {
		// while there's more data and hasn't received all ACKs
		//	1. send data up to the smaller of cwnd and rwnd
		//	2. wait for ACKs
		printf("##################################\n");

		limit = (cwnd < rwnd) ? cwnd : rwnd;
		availpack = (limit - bytesend) / PACKSIZE;
		printf("availpack = %d\n", availpack);
		
		// send new data
		printf("############# new data ##############\n");
		for (i = 0; i < availpack; i++) {
			if (bytesread == fsize) {
				nodata = 1;
				break;
			}
			memset(packet, 0, PACKSIZE);
			// read data
			lseek(filefd, foffset, SEEK_SET);
			if ((bytesread = read(filefd, packet + HEADERSIZE, DATASIZE)) < 0) {
				fprintf(stderr, "Error reading file\n");
				return -1;
			}
			printf("Reading at offset %zd for %zd bytes\n", foffset, bytesread);

			// set header
			self->seq = initseq + foffset % MAXSEQNUM;
			self->flag = bytesread << 3;
			// log operation as wnditem_t
			gettimeofday(&curtime, NULL);
			add_witem(&witems, foffset, self->seq, bytesread, &curtime);
			foffset += bytesread;

			// send packet
			fprintf(stdout, "Sending data packet %hu %hu %hu\n", self->seq, cwnd, ssthresh);
			if ((bsend = send_packet(packet, hinfo, self, other)) < 0) {
				fprintf(stderr, "Error sending packet\n");
				return -1;
			}
			bytesend += bsend;
		}

		printf("################ check timer ############\n");
		// check timer and retransmit old data
		for (i = 0; i < witems.nitems; i++) {
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
				fprintf(stdout, "Sending data packet %hu %hu %hu Retransmission timeout\n", self->seq, cwnd, ssthresh);
				if (send_packet(packet, hinfo, self, other) < 0) {
					fprintf(stderr, "Error sending retransmission packet\n");
					return -1;
				}
			}
		}

		printf("################ check dup ################\n");
		// check for duplicate ACKs
		if (nacked > 3) {
			cwnd = INITCWND;
			tempoffset = acknum;
			while (tempoffset != foffset) {
				memset(packet, 0, PACKSIZE);
				for (i = 0; i < witems.nitems; i++) {
					if (tempoffset == witems.list[i].offset) {
						printf("Rereading\n");
						lseek(filefd, tempoffset, SEEK_SET);
						if ((bytesread = read(filefd, packet + HEADERSIZE, witems.list[i].datalen)) < 0) {
							fprintf(stderr, "Error reading file\n");
							return -1;
						}
						self->seq = witems.list[i].seq;
						self->flag = witems.list[i].datalen << 3;
						gettimeofday(&curtime, NULL);
						update_timer(&witems, i, &curtime);
						tempoffset += witems.list[i].datalen;
						fprintf(stderr, "Sending data packet %hu %hu %hu Retransmission dup\n", self->seq, cwnd, ssthresh);
						if (send_packet(packet, hinfo, self, other) < 0) {
							fprintf(stderr, "Error sending retransmission packet\n");
							return -1;
						}
						goto NEXT;
					}
				}
					lseek(filefd, tempoffset, SEEK_SET);
				resendlen = (tempoffset + DATASIZE < foffset) ? DATASIZE : foffset - tempoffset;
				if ((bytesread = read(filefd, packet + HEADERSIZE, resendlen)) < 0) {
					fprintf(stderr, "Error reading file\n");
					return -1;
				}
				self->seq = initseq + tempoffset % MAXSEQNUM;
				self->flag = bytesread << 3;
				gettimeofday(&curtime, NULL);
				add_witem(&witems, tempoffset, self->seq, bytesread, &curtime);
				tempoffset += resendlen;
				fprintf(stderr, "Sending data packet %hu %hu %hu\n dup unlog", self->seq, cwnd, ssthresh);
				if (send_packet(packet, hinfo, self, other) < 0) {
					fprintf(stderr, "Error sending retransmission packet\n");
				}
				NEXT: continue;
			}
		}
		//end--;
		end = nodata && (bytesacked == fsize);
	}


	return -1;
}

int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other)
{
	// file write control parameters
	off_t foffset = 0;

	// packet and buffering parameters
	unsigned char packet[PACKSIZE];
	uint16_t initack = other->seq + 1;
	uint16_t nextack = initack;
	bufitempool_t bitems;

	// thread control
	pthread_t tid;
	int thrdstop = 0;

	// misc
	int end = 0;
	int i = 0;

	// creating listening thread
	recvudata_t ud;
	ud.hinfo = hinfo;
	ud.self = self;
	ud.other = other;
	ud.thrdstop = &thrdstop;
	ud.bitems = &bitems;
	ud.initack = initack;
	ud.nextack = &nextack;
	ud.foffset = &foffset;

	if (pthread_create(&tid, NULL, listen_datapacket, &ud) != 0) {
		fprintf(stderr, "Error: cannot create new thread\n");
		return -1;
	}

	bitems.nitems = 0;
	bitems.size = MAXSEQNUM / DATASIZE;
	bitems.list = calloc(bitems.size, sizeof(bufitem_t));

	lseek(filefd, 0, SEEK_SET);

	
	while (!end) {
		// check list and write data continuously
		if (bitems.nitems == 0)
			continue;
		for (i = 0; i < bitems.nitems; i++) {
			if (bitems.list[i].offset != foffset)
				continue;
			// write next chunk of data
			lseek(filefd, foffset, SEEK_SET);
			if (write(filefd, bitems.list[i].data, bitems.list[i].datalen) < 0) {
				fprintf(stderr, "Error: cannot write to file\n");
			}
			// update new file offset
			foffset += bitems.list[i].datalen;
			// remove the written item from buffer
			remove_bitem(&bitems, i);
		}
		end = (foffset == fsize);
	}
	return -1;
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
		fprintf(stderr, "Receiving ACK packet %hu\n", other->ack);
		printf("acknum = %d, received ack = %hu\n", *acknum, other->ack);
		if (*acknum == other->ack) {
			*nacked += 1;
		} else {
			*acknum = other->ack;
			*nacked = 1;
		}

		for (i = 0; i < witems->nitems; i++) {
			if ((witems->list[i].seq < *acknum) || 
				((witems->list[i].seq > *acknum) && (witems->list[i].seq - *acknum > OFTHRESH))) {
				*bytesend -= witems->list[i].datalen + HEADERSIZE;
				remove_witem(witems, i);
			}
		}

		// modify congestion window
		if (*cwnd < *ssthresh) {
			*cwnd *= 2;
		} else {
			*cwnd += 1;
		}


	}
	
	return NULL;
}

static void *listen_datapacket(void *userdata)
{
	// retrieve passed data from userdata
	recvudata_t *ud = (recvudata_t *)userdata;
	hostinfo_t *hinfo = ud->hinfo;
	conninfo_t *self = ud->self;
	conninfo_t *other = ud->other;
	int *thrdstop = ud->thrdstop;
	bufitempool_t *bitems = ud->bitems;
	uint16_t initack = ud->initack;
	uint16_t *nextack = ud->nextack;
	off_t *foffset = ud->foffset;

	unsigned char packet[PACKSIZE];
	off_t offset = 0;
	uint16_t datalen = 0;
	int multiplier = 0;
	uint16_t lastseq = initack;
	uint16_t deltaseq = 0;

	while (!*thrdstop) {
		// listen for packet
		memset(packet, 0, PACKSIZE);
		if (recv_packet(packet, hinfo, self, other) < 0) {
			fprintf(stderr, "Fatal error: cannot receive data packets, aborting\n");
			exit(2);
		}
		fprintf(stderr, "Receiving data packet %hu\n", other->seq);

		// check if seq has overflowed
		deltaseq = (lastseq < other->seq) ? other->seq - lastseq : lastseq - other->seq;
		if (deltaseq > OFTHRESH)
			multiplier += 1;

		offset = (unsigned)other->seq + multiplier * MAXSEQNUM - initack;
		datalen = other->flag >> 3;
		
		// check if the packet data is already received
		if (offset < *foffset) {
			self->ack = *nextack;
			self->flag = (datalen << 3) | ACK;
			memset(packet, 0, PACKSIZE);
			fprintf(stderr, "Sending ACK packet %hu Retransmission\n", *nextack);
			if (send_packet(packet, hinfo, self, other) < 0) {
				fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
				exit(2);
			}
			continue;
		}
		
		add_bitem(bitems, offset, packet + HEADERSIZE, datalen);
		// send back ACK packets
		*nextack = (offset == *foffset) ? other->seq + datalen : *nextack;
		self->ack = *nextack;
		self->flag = (datalen << 3) | ACK;
		memset(packet, 0, PACKSIZE);
		fprintf(stderr, "Sending ACK packet %hu\n", self->ack);
		if (send_packet(packet, hinfo, self, other) < 0) {
			fprintf(stderr, "Fatal error: cannot send ACK packet, aborting\n");
			exit(2);
		}
	}

	return NULL;
}

static void add_witem(wnditempool_t *witems, off_t offset, uint16_t seq, uint16_t datalen, struct timeval *tv)
{
	if (witems->nitems == witems->size) {
		witems->size *= 2;
		witems->list = realloc(witems->list, witems->size);
	}

	(witems->list)[witems->nitems].offset = offset;
	(witems->list)[witems->nitems].seq = seq;
	(witems->list)[witems->nitems].datalen = datalen;
	(witems->list)[witems->nitems].nacked = 0;
	(witems->list)[witems->nitems].tv.tv_sec = tv->tv_sec;
	(witems->list)[witems->nitems].tv.tv_usec = tv->tv_usec;
	witems->nitems += 1;
	fprintf(stderr, "Logging item with offset %lld, seq %hu, datalen %hu, n = %d\n", offset, seq, datalen, witems->nitems);
}

static void add_bitem(bufitempool_t *bitems, off_t offset, unsigned char *data, uint16_t datalen)
{
	int i;
	if (bitems->nitems == bitems->size)
		return;

	for (i = 0; i < bitems->nitems; i++) {
		if (bitems->list[i].offset == offset)
			return;
	}
	(bitems->list)[bitems->nitems].offset = offset;
	(bitems->list)[bitems->nitems].datalen = datalen;
	(bitems->list)[bitems->nitems].data = malloc(datalen);
	memcpy((bitems->list)[bitems->nitems].data, data, datalen);
	bitems->nitems += 1;
	fprintf(stderr, "Buffering %lld, n = %d\n", offset, bitems->nitems);
}

static void remove_bitem(bufitempool_t *bitems, int index)
{
	memmove(&bitems->list[index], &bitems->list[index + 1], bitems->size - (index + 1));
	if (bitems->nitems != 0)
		bitems->nitems -= 1;
	memset(&bitems->list[index], 0, sizeof(bufitem_t));
	fprintf(stderr, "Removing [%d]th item from bitems, n = %i\n", index, bitems->nitems);
}

static void remove_witem(wnditempool_t *witems, int index)
{
	memmove(&witems->list[index], &witems->list[index + 1], witems->size - (index + 1));
	if (witems->nitems != 0)
		witems->nitems -= 1;
	memset (&witems->list[index], 0, sizeof(wnditem_t));
	fprintf(stderr, "Removing [%d]th item from witems, n = %d\n", index, witems->nitems);
}

static void update_timer(wnditempool_t *witems, int index, struct timeval *tv)
{
	memcpy(&(witems->list)[index].tv, tv, sizeof(struct timeval));
}