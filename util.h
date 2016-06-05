#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <arpa/inet.h>

#define PACKSIZE		1024
#define DATASIZE		1008
#define HEADERSIZE		8
#define MAGICSIZE		16
#define FIN				0x1
#define SYN 			0x2
#define	ACK				0x4
#define MAXSEQNUM		30720
#define TIMEOUT			500			// milliseconds
#define INITRWND 		30720
#define INITCWND		PACKSIZE
#define INITSSTHRESH	65535
#define OFTHRESH		MAXSEQNUM-DATASIZE
#define TIMETHRESH		8192
#define SECTOUSEC		1000000
#define MSECTOUSEC		1000

int is_numeric(const char *str);
int is_ip_format(const char *str);
int convert_to_ip(const char *host, char **ip);
unsigned short init_seqnum();
void ushort_to_string(uint16_t *ushort, unsigned char *str);
void string_to_ushort(unsigned char *str, uint16_t *ushort);
void uint_to_string(uint32_t *fsize, unsigned char *str);
void string_to_uint(unsigned char *str, uint32_t *fsize);


#endif