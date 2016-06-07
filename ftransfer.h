#include <sys/time.h> 
#include "helper.h"
#include "util.h"

typedef enum {
	T_RESENDREQ,
	T_RESENDBEG,
	T_RESENDFIN
} resendstat_t;

typedef struct {
	off_t offset;
	uint32_t seq;
	uint16_t datalen;
	struct timeval tv;
} wnditem_t;

typedef struct {
	wnditem_t *list;
	int head;
	int tail;
	int nitems;
	int size;
} wnditempool_t;

typedef struct {
	hostinfo_t *hinfo;
	conninfo_t *self;
	conninfo_t *other;
	int filefd;
	int *thrdstop;
	uint32_t *cwnd;
	uint32_t *ssthresh;
	uint16_t *rwnd;
	ssize_t *bytesonwire;
	off_t *sendoffset;
	off_t *recvoffset;
	resendstat_t *resendstat;
	wnditempool_t *witems;
} sendudata_t;

typedef struct {
	off_t offset;
	uint32_t seq;
	unsigned char *data;
	uint16_t datalen;
} bufitem_t;

typedef struct {
	bufitem_t *list;
	int head;
	int tail;
	int nitems;
	int size;
} bufitempool_t;

typedef struct {
	hostinfo_t *hinfo;
	conninfo_t *self;
	conninfo_t *other;
	uint16_t *initack;
	off_t *recvoffset;
	off_t *bufoffset;
	bufitempool_t *bitems;
	int *eof;
} recvudata_t;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);
static void *listen_ackpacket(void *userdata);
static void *listen_datapacket(void *userdata);
static void add_witem(wnditempool_t *witems, off_t offset, uint32_t seq, uint16_t datalen, struct timeval *tv);
static void remove_witem(wnditempool_t *witems, int index);
static void add_bitem(bufitempool_t *bitems, off_t offset, uint32_t seq, unsigned char *data, uint16_t datalen);
static void remove_bitem(bufitempool_t *bitems, int index);