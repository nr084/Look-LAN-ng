#ifndef HOST_H
#define HOST_H
#include "network.h"
typedef enum { OS_UNKNOWN, OS_WINDOWS, OS_LINUX, OS_MACOS, OS_BSD, OS_NETWORK_DEVICE, OS_PRINTER, OS_NAS, OS_EMBEDDED } os_family_t;
typedef struct {
    ip_address_t address;
    char hostname[256];
    uint8_t mac[6];
    bool has_mac, online, cache_only;
    os_family_t os_family;
    uint8_t os_confidence, ttl;
    uint32_t latency_ms, interface_index;
    bool has_latency;
    uint64_t first_seen, last_seen;
    uint16_t open_ports[64];
    uint8_t port_count;
    bool ports_checked, ports_truncated;
    bool new_device, changed, ip_correlated;
    uint8_t missed_rounds;
    uint64_t seen_round;
} host_info_t;
/* Caller owns storage; upsert merges only known fields. No hidden allocation. */
size_t host_merge(host_info_t *hosts, size_t *count, size_t capacity, const host_info_t *incoming);
#endif
