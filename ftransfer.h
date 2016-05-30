#include <sys/time.h>
#include "helper.h"
#include "util.h"

typedef enum {
	PACK_UNSENT,
	PACK_SENT,
	PACK_ACKED
} sendstat_t;

typedef struct {
	uint16_t seq;
	sendstat_t stat;
	struct timeval tv;
	unsigned char data[DATASIZE];
	uint16_t datalen;
} swnditem_t;

typedef enum {
	PACK_UNRECVED,
	PACK_BUFFERED
} recvstat_t;

typedef struct {
	uint16_t seq;
	recvstat_t stat;
	unsigned char data[DATASIZE];
	uint16_t datalen;
} rwnditem_t;

typedef struct {
	hostinfo_t *hinfo;
	conninfo_t *self;
	conninfo_t *other;
	int *thrdstop;
	swnditem_t *witems;
	unsigned *wsize;
	unsigned *cwnd;
	unsigned *ssthresh;
	unsigned *head;
	unsigned *tail;
	swnditem_t *lastpack;
	int *nacked;
} userdata_t;



typedef struct {
	unsigned offset;
	uint16_t seq;
	uint16_t datalen;
	int nacked;
	struct timeval tv;
} witem_t;

typedef struct {
	witem_t *list;
	int nitems;
	int size;
} witempool_t;

typedef struct {
	hostinfo_t *hinfo;
	conninfo_t *self;
	conninfo_t *other;
	int *thrdstop;
	uint16_t *cwnd;
	uint16_t *ssthresh;
	uint16_t *rwnd;
	witempool_t *pool;
} userdata;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, size_t fsize, conninfo_t *self, conninfo_t *other);

static void *listen_packet(void* userdata);
static void add_item(witempool_t *pool, unsigned offset, uint16_t seq, uint16_t datalen, struct timeval *tv);
static void update_timer(witempool_t *pool, int index, struct timeval *tv);