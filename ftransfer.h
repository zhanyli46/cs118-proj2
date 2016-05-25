#include <sys/time.h>
#include "helper.h"
#include "util.h"

typedef enum {
	PACK_UNSENT,
	PACK_SENT,
	PACK_ACKED
} packstat_t;

typedef struct {
	uint16_t seq;
	packstat_t stat;
	struct timeval tv;
	unsigned char data[DATASIZE];
	uint16_t datalen;
} wnditem_t;

typedef struct {
	hostinfo_t *hinfo;
	conninfo_t *self;
	conninfo_t *other;
	int *thrdstop;
	wnditem_t *witems;
	unsigned *wsize;
	unsigned *cwnd;
	unsigned *ssthresh;
	unsigned *head;
	unsigned *tail;
	wnditem_t *lastpack;
	int *nacked;
} userdata_t;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
static int readdata(int filefd, wnditem_t *item);
static void *recv_ack(void *userdata);