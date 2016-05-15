#ifndef _UTIL_H_
#define _UTIL_H_

int is_numeric(const char *str);
int is_ip_format(const char *str);
int convert_to_ip(const char *host, char *ip);

#endif