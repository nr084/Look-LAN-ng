#ifndef SCANNER_H
#define SCANNER_H
#include "host.h"
typedef enum { SCAN_OK, SCAN_TIMEOUT, SCAN_UNREACHABLE, SCAN_REFUSED, SCAN_NO_ROUTE, SCAN_CANCELLED, SCAN_SYSTEM_ERROR } scan_result_t;
typedef struct platform_net_ops {
    scan_result_t (*resolve_neighbor)(void *, uint32_t, lan_host_t *);
    scan_result_t (*icmp_probe)(void *, uint32_t, uint32_t, lan_host_t *);
    bool (*cancelled)(void *);
    uint64_t (*wall_seconds)(void *);
    scan_result_t (*tcp_probe)(void *, uint32_t, uint16_t, uint32_t);
} platform_net_ops_t;
scan_result_t scanner_probe(const platform_net_ops_t *ops, void *context, uint32_t ip, uint32_t timeout, lan_host_t *host);
scan_result_t scanner_services(const platform_net_ops_t *ops, void *context, lan_host_t *host, const uint16_t *ports, size_t count, uint32_t timeout);
#endif
