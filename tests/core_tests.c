#include "scanner.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
typedef struct { bool cancel; unsigned arp_calls, ping_calls; bool arp_alive, ping_alive, cancel_after_arp; } fake_t;
static scan_result_t arp(void *p, uint32_t ip, lan_host_t *h) { fake_t *f = p; (void)ip; ++f->arp_calls; if (f->cancel_after_arp) f->cancel = true; if (f->arp_alive) { h->has_mac = true; h->mac[0] = 1; return SCAN_OK; } return SCAN_UNREACHABLE; }
static scan_result_t ping(void *p, uint32_t ip, uint32_t timeout, lan_host_t *h) { fake_t *f = p; (void)ip; (void)timeout; ++f->ping_calls; if (f->ping_alive) { h->has_latency = true; h->latency_ms = 7; return SCAN_OK; } return SCAN_TIMEOUT; }
static bool cancel(void *p) { return ((fake_t *)p)->cancel; }
static uint64_t clock_fake(void *p) { (void)p; return 123; }
int main(void) {
    uint32_t ip; ipv4_range_t range;
    CHECK(ipv4_parse("192.168.1.10",12,&ip)); CHECK(ip == 0xc0a8010a);
    CHECK(!ipv4_parse("256.1.1.1",9,&ip)); CHECK(!ipv4_parse("1.2.3.4junk",11,&ip)); CHECK(!ipv4_parse("1..3.4",6,&ip));
    CHECK(!ipv4_parse("1.2.3.4\0x",9,&ip)); CHECK(!ipv4_parse(NULL,0,&ip));
    char address[32]; ipv4_format(0xc0a8010a,address,sizeof(address)); CHECK(!strcmp(address,"192.168.1.10"));
    CHECK(network_range(0xc0a8010a,24,&range)); CHECK(range.first == 0xc0a80101 && range.last == 0xc0a801fe);
    CHECK(network_range(0xc0a8010a,23,&range)); CHECK(range.first == 0xc0a80001 && range.last == 0xc0a801fe && (uint64_t)range.last-range.first+1 == 510);
    CHECK(network_range(0xc0a8010a,32,&range)); CHECK(range.first == range.last);
    CHECK(network_range(0xc0a8010a,31,&range)); CHECK(range.last-range.first == 1);
    CHECK(network_range(0,0,&range)); CHECK(range.first == 1 && range.last == UINT32_MAX-1);
    CHECK(!network_range(0,33,&range));
    platform_net_ops_t ops = {arp,ping,cancel,clock_fake,NULL}; fake_t fake = {0}; lan_host_t host;
    fake.arp_alive = true; CHECK(scanner_probe(&ops,&fake,0xc0a80101,300,&host) == SCAN_OK); CHECK(host.online && host.has_mac && !host.has_latency); CHECK(host.first_seen == 123);
    fake.arp_alive = false; fake.ping_alive = true; CHECK(scanner_probe(&ops,&fake,0xc0a80101,300,&host) == SCAN_OK); CHECK(host.has_latency && host.latency_ms == 7);
    fake.ping_alive = false; CHECK(scanner_probe(&ops,&fake,0xc0a80101,300,&host) == SCAN_TIMEOUT); CHECK(!host.online);
    fake.cancel = true; unsigned before = fake.arp_calls; CHECK(scanner_probe(&ops,&fake,1,300,&host) == SCAN_CANCELLED); CHECK(fake.arp_calls == before);
    fake.cancel = false; fake.cancel_after_arp = true; before = fake.ping_calls; CHECK(scanner_probe(&ops,&fake,1,300,&host) == SCAN_CANCELLED); CHECK(fake.ping_calls == before);
    lan_host_t hosts[2] = {0}; size_t count = 0; lan_host_t cached = {0}; cached.address = ipv4_address(0xc0a80101); cached.cache_only = true; cached.has_mac = true; cached.mac[0] = 42;
    CHECK(host_merge(hosts,&count,2,&cached) == 0); CHECK(count == 1 && !hosts[0].online);
    lan_host_t active = {0}; active.address = cached.address; active.online = true; active.last_seen = active.first_seen = 123;
    CHECK(host_merge(hosts,&count,2,&active) == 0); CHECK(count == 1 && hosts[0].has_mac && hosts[0].mac[0] == 42 && hosts[0].online && !hosts[0].cache_only);
    CHECK(hosts[0].first_seen == 123);
    lan_host_t dns = active; memcpy(dns.hostname,"router",7); dns.last_seen = 100; CHECK(host_merge(hosts,&count,2,&dns) == 0); CHECK(!strcmp(hosts[0].hostname,"router") && hosts[0].last_seen == 123 && hosts[0].has_mac);
    active.interface_index = 2; CHECK(host_merge(hosts,&count,2,&active) == 1); active.address = ipv4_address(2); CHECK(host_merge(hosts,&count,2,&active) == SIZE_MAX);
    puts("Core tests passed: address parsing, subnet boundaries, fake discovery, ICMP-blocked ARP host, cancellation, cache merge, deduplication, capacity.");
    return 0;
}
