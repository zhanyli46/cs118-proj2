#include <sys/time.h>
#include "helper.h"
#include "util.h"

typedef struct {
	unsigned offset;
	uint16_t seq;
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
	int *thrdstop;
	uint16_t *cwnd;
	uint16_t *ssthresh;
	uint16_t *rwnd;
	uint16_t *acknum;
	int *nacked;
	ssize_t *bytesend;
	wnditempool_t *witems;
} sendudata_t;

typedef struct {
	unsigned offset;
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
	bufitempool_t *bitems;
	uint16_t initack;
	uint16_t *nextack;
	off_t *foffset;
	int *end;
} recvudata_t;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);
static void *listen_ackpacket(void *userdata);
static void *listen_datapacket(void *userdata);
static void add_witem(wnditempool_t *witems, off_t offset, uint16_t seq, uint16_t datalen, struct timeval *tv);
static void remove_witem(wnditempool_t *witems, int index);
static void add_bitem(bufitempool_t *bitems, off_t offset, unsigned char  *data, uint16_t datalen);
static void remove_bitem(bufitempool_t *bitems, int index);
static void update_timer(wnditempool_t *witems, int index, struct timeval *tv);