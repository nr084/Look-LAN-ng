#include "host.h"
#include <string.h>
size_t host_merge(host_info_t *h, size_t *n, size_t cap, const host_info_t *v) {
    for (size_t i = 0; i < *n; ++i) {
        if (h[i].address.family != v->address.family || h[i].address.scope_id != v->address.scope_id ||
            h[i].interface_index != v->interface_index || memcmp(h[i].address.bytes, v->address.bytes, 16)) continue;
        if (v->has_mac) { memcpy(h[i].mac, v->mac, 6); h[i].has_mac = true; }
        if (v->hostname[0]) { memcpy(h[i].hostname, v->hostname, sizeof(v->hostname)); h[i].hostname[255] = 0; }
        if (v->has_latency) { h[i].latency_ms = v->latency_ms; h[i].ttl = v->ttl; h[i].has_latency = true; }
        if (v->ports_checked) { memcpy(h[i].open_ports,v->open_ports,sizeof(v->open_ports)); h[i].port_count = v->port_count; h[i].ports_checked = true; h[i].ports_truncated = v->ports_truncated; h[i].os_family = v->os_family; h[i].os_confidence = v->os_confidence; }
        if (v->online) { h[i].online = true; h[i].cache_only = false; if (v->last_seen > h[i].last_seen) h[i].last_seen = v->last_seen; if (!h[i].first_seen) h[i].first_seen = v->first_seen; }
        return i;
    }
    if (*n == cap) return SIZE_MAX;
    h[*n] = *v;
    h[*n].hostname[255] = 0;
    return (*n)++;
}
