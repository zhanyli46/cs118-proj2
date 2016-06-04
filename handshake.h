#include "helper.h"

int handshake_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other, size_t *fsize);
int handshake_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other);
int terminate_client(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other);
int terminate_server(hostinfo_t *hinfo, conninfo_t *self, conninfo_t *other);