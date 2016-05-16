#ifndef _UTIL_H_
#define _UTIL_H_

#define BUFSIZE 1024
typedef char packet[BUFSIZE];

int is_numeric(const char *str);
int is_ip_format(const char *str);
int convert_to_ip(const char *host, char **ip);
unsigned short init_seqnum();

#endif