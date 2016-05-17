#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include "util.h"

#include <stdio.h>


int is_numeric(const char *str)
{
	int i;
	int len = strlen(str);
	for (i = 0; i < len; i++) {
		if (str[i] < '0' || str[i] > '9')
			return 0;
	}
	return 1;
}

int is_ip_format(const char *str)
{
	int i;
	int head, tail;
	int iplen;
	int dotcount = 0;
	char ipseg[16];
	int ipsegval;


	iplen = strlen(str);
	if (iplen > 15)
		return 0;
	for (i = 0; i < iplen; i++) {
		head = i;
		while (i < iplen) {
			if (str[i] != '.') {
				i++;
				continue;
			} else {
				dotcount++;
				break;
			}
		}
		tail = i;
		memset(ipseg, 0, sizeof(ipseg));
		strncpy(ipseg, &str[head], tail - head);
		if (!is_numeric(ipseg)) 
			return 0;
		ipsegval = atoi(ipseg);
		if (ipsegval < 0 || ipsegval > 255)
			return 0;
	}
	if (dotcount != 3)
		return 0;
	return 1;
}

int convert_to_ip(const char *host, char **ip)
{
	struct hostent *hent;

	if (is_ip_format(host)) {
		*ip = malloc(strlen(host));
		strcpy(*ip, host);
		return 1;
	}
	hent = gethostbyname(host);
	if (hent == NULL) {
		return 0;
	} else {
		struct in_addr **addr_list = (struct in_addr **)hent->h_addr_list;
		*ip = malloc(strlen(inet_ntoa(*addr_list[0])));
		strcpy(*ip, inet_ntoa(*addr_list[0]));
		return 1;
	}
}

inline uint16_t init_seqnum()
{
	srand(time(0));
	return (uint16_t)rand() % 30720;
}

void ushort_to_string(uint16_t *ushort, unsigned char *str)
{
	unsigned char lo = *ushort & 0xff;
	unsigned char hi = *ushort >> 8;
	
	str[0] = hi;
	str[1] = lo;
}

void string_to_ushort(unsigned char *str, uint16_t* ushort)
{
	unsigned char hi = str[0];
	unsigned char lo = str[1];
	*ushort =  lo | ((uint16_t)hi) << 8;
}