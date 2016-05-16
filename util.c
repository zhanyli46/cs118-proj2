#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include "util.h"


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

inline unsigned short init_seqnum()
{
	srand(time(0));
	return (unsigned short)rand() % 30720;
}
