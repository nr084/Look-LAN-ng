#include "scanner.h"
#include <string.h>
#include "ports.h"
scan_result_t scanner_probe(const platform_net_ops_t *o, void *ctx, uint32_t ip, uint32_t timeout, host_info_t *h) {
    memset(h, 0, sizeof(*h));
    h->address = ipv4_address(ip);
    if (o->cancelled(ctx)) return SCAN_CANCELLED;
    scan_result_t arp = o->resolve_neighbor(ctx, ip, h);
    if (o->cancelled(ctx)) return SCAN_CANCELLED;
    scan_result_t ping = o->icmp_probe(ctx, ip, timeout, h);
    if (o->cancelled(ctx)) return SCAN_CANCELLED;
    h->online = arp == SCAN_OK || ping == SCAN_OK;
    if (!h->online && o->tcp_probe) {
        for (size_t i = 0; i < discovery_port_count; ++i) {
            if (o->cancelled(ctx)) return SCAN_CANCELLED;
            scan_result_t tcp = o->tcp_probe(ctx,ip,discovery_ports[i],250);
            if (tcp == SCAN_OK || tcp == SCAN_REFUSED) { h->online = true; break; }
            if (tcp == SCAN_CANCELLED) return tcp;
        }
    }
    if (!h->online) return ping;
    h->first_seen = h->last_seen = o->wall_seconds(ctx);
    return SCAN_OK;
}
scan_result_t scanner_services(const platform_net_ops_t *o, void *ctx, host_info_t *h, const uint16_t *ports, size_t n, uint32_t timeout) {
    if (!o->tcp_probe || !ports) return SCAN_SYSTEM_ERROR;
    h->port_count = 0; h->ports_truncated = false; h->ports_checked = false;
    bool windows = false, ssh = false, printer = false;
    for (size_t i = 0; i < n; ++i) {
        if (o->cancelled(ctx)) return SCAN_CANCELLED;
        scan_result_t result = o->tcp_probe(ctx,ipv4_value(&h->address),ports[i],timeout);
        if (result == SCAN_CANCELLED) return result;
        if (result != SCAN_OK) continue;
        if (h->port_count < 64) h->open_ports[h->port_count++] = ports[i]; else h->ports_truncated = true;
        if (ports[i] == 135 || ports[i] == 445 || ports[i] == 3389 || ports[i] == 5985 || ports[i] == 5986) windows = true;
        if (ports[i] == 22) ssh = true;
        if (ports[i] == 9100) printer = true;
    }
    h->ports_checked = true; h->os_family = OS_UNKNOWN; h->os_confidence = 0;
    if (printer) { h->os_family = OS_PRINTER; h->os_confidence = 70; }
    else if (windows && h->ttl > 64) { h->os_family = OS_WINDOWS; h->os_confidence = 65; }
    else if (ssh && h->has_latency && h->ttl <= 64) { h->os_family = OS_LINUX; h->os_confidence = 50; }
    return SCAN_OK;
}
