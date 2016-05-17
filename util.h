#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define FIN 	0x1
#define SYN 	0x2
#define	ACK		0x4

int is_numeric(const char *str);
int is_ip_format(const char *str);
int convert_to_ip(const char *host, char **ip);
unsigned short init_seqnum();
void ushort_to_string(uint16_t *ushort, unsigned char *str);
void string_to_ushort(unsigned char *str, uint16_t *ushort);

#endif