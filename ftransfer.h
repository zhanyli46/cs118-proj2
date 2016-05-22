#include "helper.h"

int ftransfer_sender(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);
int ftransfer_recver(hostinfo_t *hinfo, int filefd, conninfo_t *self, conninfo_t *other);