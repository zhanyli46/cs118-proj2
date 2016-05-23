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
	char data[DATASIZE];
	uint16_t datalen;
} wnditem_t;

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
static int readdata(int filefd, wnditem_t *item);