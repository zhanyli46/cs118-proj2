#include <string.h>
#include <stdio.h>	
#include <unistd.h>
#include "handshake.h"

int handshake_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other, size_t *fsize)
{	
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;
	uint16_t nextseq;

	// set connection parameters
	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = SYN;
	memset(packet, 0, PACKSIZE);

	// send SYN + sequence number
	fprintf(stdout, "Sending SYN packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		return -1;


	memset(packet, 0, PACKSIZE);
	// recv packet and check flag (SYN + ACK)
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0)
			return -1;
		if ((other->flag & (SYN | ACK))) {
			fprintf(stdout, "Receiving SYN/ACK packet\n");
			nextseq = other->seq + 1;
			break;
		} else { 
			continue;
		}
	}


	// set connection parameters
	self->seq += 1;
	self->flag = ACK;
	self->ack = nextseq;
	memset(packet, 0, PACKSIZE);

	// send ACK
	fprintf(stdout, "Sending ACK packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		return -1;
	return 0;
}

int handshake_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;
	uint16_t nextseq;
	size_t fsize = 0;

	fsize = self->seq * 65536 + self->ack;

	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = 0;

START:
	// recv for the first time, expecting SYN; store client information in hinfo
	memset(packet, 0, PACKSIZE);
	while ((inbytes = recvfrom(hinfo->sockfd, packet, PACKSIZE, 0, (struct sockaddr *)hinfo->addr, &hinfo->addrlen) >= 0)) {
		interpret_header(packet, &other->seq, &other->ack, &other->rwnd, &other->flag);
		
		if (other->flag & SYN) {
			// received SYN (TCP-like connection initiation)
			fprintf(stdout, "Receiving SYN packet\n");
			nextseq = other->seq + 1;
			break;
		} else {
			// unknown flag, ignore packet
			continue;
		}
	}
	if (inbytes < 0)
		return -1;

	//set connection parameters
	self->flag = SYN | ACK;
	self->ack = nextseq;
	memset(packet, 0, PACKSIZE);

	// send SYN + ACK
	fprintf(stdout, "Sending SYN/ACK packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		goto START;
	

	memset(packet, 0, PACKSIZE);
	// recv packet and check flag (ACK)
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0)
			return -1;
		if ((other->flag & ACK) && (other->seq == nextseq)) {
			fprintf(stdout, "Receiving ACK packet\n");
			break;
		} else { 
			continue;
		}
	}
	self->seq += 1;
	return 0;
}


int terminate_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;

	memset(packet, 0, PACKSIZE);
	self->flag = ACK;
	fprintf(stderr, "Sending ACK packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0) {
		fprintf(stderr, "Error sending ACK packet\n");
		return -1;
	}

	memset(packet, 0, PACKSIZE);
	self->flag = FIN;
	fprintf(stderr, "Sending FIN packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0) {
		fprintf(stderr, "Error sending FIN packet\n");
		return -1;
	}

	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0) {
			fprintf(stderr, "Error receiving ACK packet\n");
			return -1;
		} else if (other->flag == ACK) {
			fprintf(stderr, "Receiving ACK packet\n");
			break;
		} else {
			continue;
		}
	}

	return 0;
}

int terminate_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;
	
	memset(packet, 0, PACKSIZE);
	self->flag = FIN;
	fprintf(stderr, "Sending FIN packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0) {
		fprintf(stderr, "Error sending FIN packet\n");
		return -1;
	}

	memset(packet, 0, PACKSIZE);
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0) {
			fprintf(stderr, "Error receiving ACK reply\n");
			return -1;
		} else if (other->flag == ACK) {
			fprintf(stderr, "Receiving ACK packet\n");
			break;
		} else {
			continue;
		}
	}

	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0) {
			fprintf(stderr, "Error receiving FIN packet\n");
			return -1;
		} else if (other->flag == FIN) {
			fprintf(stderr, "Receiving FIN packet\n");
			break;
		} else {
			continue;
		}
	}

	memset(packet, 0, PACKSIZE);
	self->flag = ACK;
	fprintf(stderr, "Sending ACK packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0) {
		fprintf(stderr, "Error sending ACK packet\n");
		return -1;
	}

	return 0;
}
