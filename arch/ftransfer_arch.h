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

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
static int readdata(int filefd, swnditem_t *item);
static void *recv_ack(void *userdata);