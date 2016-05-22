#include <string.h>
#include <stdio.h>	
#include "handshake.h"

int handshake_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{	
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;

	// set connection parameters
	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = SYN;

	// send SYN + sequence number
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		return 1;

	// recv packet and check flag (SYN + ACK)
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0)
			return 1;
		if (other->flag & (SYN | ACK))
			break;
		else 
			continue;
	}

	// set connection parameters
	self->flag = ACK;
	self->ack = other->seq;

	// send ACK
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		return 1;
	
	return 0;
}

int handshake_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;

	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = 0;

START:
	// recv for the first time, expecting SYN; store client information in hinfo
	memset(packet, 0, PACKSIZE);
	while ((inbytes = recvfrom(hinfo->sockfd, packet, PACKSIZE, 0, (struct sockaddr *)hinfo->addr, &hinfo->addrlen) >= 0)) {
		interpret_header(packet, &other->seq, &other->ack, &other->rwnd, &other->flag);
		
		printf("receiving packet:\n");
		printf("other: %hu %hu %hu %hu\n", other->seq, other->ack, other->rwnd, other->flag);

		if (other->flag & SYN)
			// received SYN (TCP-like connection initiation)
			break;
		else
			// unknown flag, ignore packet
			continue;
	}
	if (inbytes < 0)
		return 1;

	//set connection parameters
	self->flag = SYN | ACK;
	self->ack = other->seq;
	
	// send SYN + ACK
	if ((outbytes = send_packet(packet, hinfo, self, other)) < 0)
		goto START;
	
	// recv packet and check flag (ACK)
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0)
			return 1;
		if (other->flag & ACK)
			break;
		else 
			continue;
	}

	return 0;
}
