#include "monitor.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static lan_host_t host(uint32_t ip, unsigned mac) {
    lan_host_t h = {0}; h.address = ipv4_address(ip); h.interface_index = 1; h.has_mac = true; h.mac[0] = 0; h.mac[1] = 0x11; h.mac[2] = 0x22; h.mac[5] = (uint8_t)mac; return h;
}
static unsigned events(const monitor_t *m, monitor_event_type_t type) {
    unsigned count = 0; for (size_t i = 0; i < m->log_count; ++i) if (monitor_log_at(m,i)->type == type) ++count; return count;
}
int main(void) {
    lan_host_t hosts[8]; size_t count; monitor_event_t log[128]; monitor_t m;
    ipv4_range_t range = {0xc0a80101,0xc0a801fe};
    monitor_init(&m,hosts,&count,8,log,128);
    lan_host_t a = host(0xc0a80101,1); a.cache_only = true;
    monitor_begin(&m,range,1,true,100); CHECK(monitor_observe(&m,&a,false,100) == 0); CHECK(!hosts[0].online && !hosts[0].first_seen);
    memcpy(a.hostname,"router",7); CHECK(monitor_observe(&m,&a,true,101) == 0); CHECK(hosts[0].online && hosts[0].first_seen == 101 && !hosts[0].new_device);
    monitor_finish(&m,true,102); CHECK(events(&m,MON_HOST_ONLINE) == 1 && m.baseline);
    monitor_begin(&m,range,1,true,110); monitor_finish(&m,true,111); CHECK(hosts[0].online && hosts[0].missed_rounds == 1);
    monitor_begin(&m,range,1,true,120); monitor_finish(&m,false,121); CHECK(hosts[0].online && hosts[0].missed_rounds == 1);
    monitor_begin(&m,range,1,true,130); monitor_finish(&m,true,131); CHECK(!hosts[0].online && events(&m,MON_HOST_OFFLINE) == 1);
    a.online = true; a.last_seen = 999; CHECK(monitor_observe(&m,&a,false,132) == 0); CHECK(!hosts[0].online && hosts[0].last_seen == 101);
    monitor_begin(&m,range,1,true,140); CHECK(monitor_observe(&m,&a,true,141) == 0); CHECK(hosts[0].online && !hosts[0].missed_rounds && hosts[0].first_seen == 101); CHECK(events(&m,MON_HOST_ONLINE) == 2);
    lan_host_t b = host(0xc0a80102,2); CHECK(monitor_observe(&m,&b,true,142) == 1); CHECK(hosts[1].new_device && events(&m,MON_NEW_HOST) == 1);
    monitor_acknowledge(&m,1); CHECK(!hosts[1].new_device && !hosts[1].changed);
    memcpy(a.hostname,"router2",8); monitor_observe(&m,&a,false,143); CHECK(hosts[0].changed && events(&m,MON_HOSTNAME_CHANGED) == 1);
    a.mac[5] = 3; monitor_observe(&m,&a,true,144); CHECK(hosts[0].changed && hosts[0].new_device && events(&m,MON_MAC_CHANGED) == 1);
    /* Late DNS carries an old MAC and online snapshot: neither may undo a
       confirmed MAC replacement or resurrect an offline host. */
    a.mac[5] = 1; monitor_observe(&m,&a,false,145); CHECK(hosts[0].mac[5] == 3);
    monitor_finish(&m,true,146);
    ipv4_range_t single = {0xc0a80102,0xc0a80102}; monitor_begin(&m,single,1,true,150); monitor_observe(&m,&b,true,151); monitor_finish(&m,true,152); CHECK(hosts[0].missed_rounds == 0);
    monitor_init(&m,hosts,&count,8,log,128); a = host(0xc0a80101,1); b = host(0xc0a80102,1);
    monitor_begin(&m,range,1,true,200); monitor_observe(&m,&a,true,201); monitor_finish(&m,true,202);
    monitor_begin(&m,range,1,true,210); monitor_observe(&m,&b,true,211); monitor_finish(&m,true,212);
    monitor_begin(&m,range,1,true,220); monitor_observe(&m,&b,true,221); monitor_finish(&m,true,222);
    CHECK(!hosts[0].online && hosts[1].online && events(&m,MON_IP_CHANGED) == 1);
    monitor_begin(&m,range,1,true,230); monitor_observe(&m,&b,true,231); monitor_finish(&m,true,232); CHECK(events(&m,MON_IP_CHANGED) == 1);
    /* Two active IPs sharing a MAC are ambiguous and must not be merged. */
    monitor_init(&m,hosts,&count,8,log,128); lan_host_t c = host(0xc0a80103,1);
    monitor_begin(&m,range,1,true,300); monitor_observe(&m,&a,true,301); monitor_finish(&m,true,302);
    for (unsigned cycle = 0; cycle < 2; ++cycle) { monitor_begin(&m,range,1,true,310+cycle*10); monitor_observe(&m,&b,true,311+cycle*10); monitor_observe(&m,&c,true,311+cycle*10); monitor_finish(&m,true,312+cycle*10); }
    CHECK(count == 3 && events(&m,MON_IP_CHANGED) == 0);
    monitor_init(&m,hosts,&count,8,log,3);
    for (unsigned i = 0; i < 8; ++i) monitor_note(&m,MON_SCAN_STARTED,"test",i+1);
    CHECK(m.log_count == 3 && monitor_log_at(&m,0)->sequence == 6 && monitor_log_at(&m,2)->sequence == 8 && !monitor_log_at(&m,3));
    puts("Monitor tests passed: baseline/NEW, duplicate observations, ONLINE-OFFLINE-ONLINE, two-miss threshold, cancellation, stale DNS/cache, MAC/name/IP changes, scoped rescan, proxy-ARP ambiguity and bounded log.");
    return 0;
}
