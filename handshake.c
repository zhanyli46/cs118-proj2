#include <string.h>
#include <stdio.h>	
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "handshake.h"

int handshake_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other, size_t *fsize)
{	
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;
	uint16_t nextseq;
	struct timeval begin, end;
	int bytesavail = -1;
	int resend = 0;

	// set connection parameters
	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = SYN;
	memset(packet, 0, PACKSIZE);

	// send SYN + sequence number
	CRESEND1:
	fprintf(stdout, "Sending SYN packet\n");
	gettimeofday(&begin, NULL);
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0)
		return -1;

	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			gettimeofday(&end, NULL);
			if ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000) {
				resend = 1;
				goto CRESEND1;
			}
		} else if (bytesavail < 1) {
			// error
			return -1;
		} else {
			break;
		}
	}
	
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
			memset(packet, 0, PACKSIZE);
			continue;
		}
	}


	// set connection parameters
	self->seq += 1;
	self->flag = ACK;
	self->ack = nextseq;
	memset(packet, 0, PACKSIZE);


	CRESEND2:
	// send ACK
	if (resend == 0)
		fprintf(stdout, "Sending ACK packet\n");
	else {
		fprintf(stdout, "Sending ACK packet Retransmission\n");
		resend = 0;
	}
	gettimeofday(&begin, NULL);
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0)
		return -1;

	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			gettimeofday(&end, NULL);
			if ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000) {
				resend = 1;
				goto CRESEND2;
			}
		} else if (bytesavail < 0) {
			// error
			return -1;
		} else {
			break;
		}
	}
	return 0;
}

int handshake_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other)
{
	unsigned char packet[PACKSIZE];
	ssize_t inbytes, outbytes;
	uint16_t nextseq;
	size_t fsize = 0;
	struct timeval begin, end;
	int bytesavail = -1;
	int resend = 0;

	fsize = self->seq * 65536 + self->ack;

	self->seq = init_seqnum();
	self->ack = 0;
	self->flag = 0;



START:
	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			continue;
		} else if (bytesavail < 1) {
			// error
			return -1;
		} else {
			break;
		}
	}

	// recv for the first time, expecting SYN; store client information in hinfo
	memset(packet, 0, PACKSIZE);
	memset(other, 0, sizeof(conninfo_t));
	while ((inbytes = recvfrom(hinfo->sockfd, packet, PACKSIZE, 0, (struct sockaddr *)hinfo->addr, &hinfo->addrlen))) {
		interpret_header(packet, &other->seq, &other->ack, &other->rwnd, &other->flag);
		if (inbytes < 0) {
			perror("recvfrom");
		}
		if (other->flag & SYN) {
			// received SYN (TCP-like connection initiation)
			fprintf(stdout, "Receiving SYN packet\n");
			nextseq = other->seq + 1;
			break;
		} else {
			// unknown flag, ignore packet
			memset(packet, 0, PACKSIZE);
			continue;
		}
	}
	if (inbytes < 0)
		return -1;

	//set connection parameters
	self->flag = SYN | ACK;
	self->ack = nextseq;
	memset(packet, 0, PACKSIZE);

	SRESEND1:
	// send SYN + ACK
	if (resend == 0)
		fprintf(stdout, "Sending SYN/ACK packet\n");
	else
		fprintf(stdout, "Sending SYN/ACK packet Retransmission\n");
	gettimeofday(&begin, NULL);
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0)
		goto START;


	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			gettimeofday(&end, NULL);
			if ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000) {
				resend = 1;
				goto SRESEND1;
			}
		} else if (bytesavail < 0) {
			// error
			return -1;
		} else {
			break;
		}
	}
	

	memset(packet, 0, PACKSIZE);
	// recv packet and check flag (ACK)
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0)
			return -1;
		if ((other->flag & ACK) && (other->seq == nextseq)) {
			fprintf(stdout, "Receiving ACK packet\n");
			break;
		} else { 
			memset(packet, 0, PACKSIZE);
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
	struct timeval begin, end;
	int bytesavail = 0;
	int resend = 0;

	memset(packet, 0, PACKSIZE);
	self->flag = ACK | FIN;
TCRESEND1:
	gettimeofday(&begin, NULL);	
	if (resend == 0)
		fprintf(stderr, "Sending ACK/FIN packet\n");
	else 
		fprintf(stderr, "Sending ACK/FIN packet Retransmission\n");
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0) {
		fprintf(stderr, "Error sending ACK packet\n");
		return -1;
	}

	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			gettimeofday(&end, NULL);
			if (resend == 0 && ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000)) {
				resend = 1;
				goto TCRESEND1;
			} else if (resend == 0 && ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000)) {
				// assume the connection is terminated
				fprintf(stderr, "Receiving ACK packet\n");
				return 0;
			}
		} else if (bytesavail < 0) {
			// error
			return -1;
		} else {
			break;
		}
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
	struct timeval begin, end;
	int bytesavail = 0;
	int resend = 0;

	memset(packet, 0, PACKSIZE);
	self->flag = FIN;

	STRESEND1:
	if (resend == 0)
		fprintf(stderr, "Sending FIN packet\n");
	else
		fprintf(stderr, "Sending FIN packet Retransmission\n");
	gettimeofday(&begin, NULL);
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0) {
		fprintf(stderr, "Error sending FIN packet\n");
		return -1;
	}
	
	while (1) {
		// check socket buffer
		ioctl(hinfo->sockfd, FIONREAD, &bytesavail);
		if (bytesavail == 0) {
			gettimeofday(&end, NULL);
			if ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) > TIMEOUT * 1000) {
				resend = 1;
				goto STRESEND1;
			}
		} else if (bytesavail < 0) {
			// error
			return -1;
		} else {
			break;
		}
	}

	memset(packet, 0, PACKSIZE);
	while ((inbytes = recv_packet(packet, hinfo, self, other)) >= 0) {
		if (inbytes < 0) {
			fprintf(stderr, "Error receiving ACK/FIN reply\n");
			return -1;
		} else if (other->flag == ACK | FIN) {
			fprintf(stderr, "Receiving ACK/FIN packet\n");
			break;
		} else {
			continue;
		}
	}

	memset(packet, 0, PACKSIZE);
	self->flag = ACK;
	fprintf(stderr, "Sending ACK packet\n");
	if ((outbytes = send_packet(packet, hinfo, self, other, PACKSIZE)) < 0) {
		fprintf(stderr, "Error sending ACK packet\n");
		return -1;
	}

	return 0;
}
