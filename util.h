#ifndef _UTIL_H_
#define _UTIL_H_

int is_numeric(const char *str);
int is_host_or_ip(const char *str);
char *convert_to_ip(const char* str);

#endif