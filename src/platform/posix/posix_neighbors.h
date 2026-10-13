#ifndef POSIX_NEIGHBORS_H
#define POSIX_NEIGHBORS_H
#include "posix_net.h"
bool posix_neighbors_open(posix_backend_t *backend);
void *posix_neighbors_watch(void *context);
scan_result_t posix_neighbor_probe(posix_backend_t *backend,uint32_t ip,lan_host_t *host);
#endif
