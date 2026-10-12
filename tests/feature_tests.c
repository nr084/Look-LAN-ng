#include "scanner.h"
#include "ports.h"
#include "vendor.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
typedef struct { bool cancelled; scan_result_t result; unsigned calls; } fake_t;
static scan_result_t arp(void *p, uint32_t ip, lan_host_t *h) { (void)p;(void)ip;(void)h;return SCAN_UNREACHABLE; }
static scan_result_t ping(void *p, uint32_t ip, uint32_t timeout, lan_host_t *h) { (void)p;(void)ip;(void)timeout;(void)h;return SCAN_TIMEOUT; }
static bool cancel(void *p) { return ((fake_t *)p)->cancelled; }
static uint64_t clock_fake(void *p) { (void)p;return 123; }
static scan_result_t tcp(void *p, uint32_t ip, uint16_t port, uint32_t timeout) {
    fake_t *f = p; (void)ip;(void)timeout; ++f->calls;
    if (f->result == SCAN_OK) return port == 80 || port == 445 ? SCAN_OK : SCAN_REFUSED;
    return f->result;
}
int main(void) {
    uint16_t ports[1024]; size_t count = 999;
    CHECK(ports_parse("20-25,80,443,80",15,ports,1024,&count)); CHECK(count == 8 && ports[0] == 20 && ports[7] == 443);
    CHECK(ports_parse(" 80 , 443 ",10,ports,1024,&count)); CHECK(count == 2);
    CHECK(!ports_parse("0,80",4,ports,1024,&count)); CHECK(!ports_parse("65536",5,ports,1024,&count));
    CHECK(!ports_parse("100-1",5,ports,1024,&count)); CHECK(!ports_parse("80,",3,ports,1024,&count));
    CHECK(!ports_parse("1-1025",6,ports,1024,&count)); CHECK(!ports_parse("80\0,443",7,ports,1024,&count));
    CHECK(ports_parse("65535",5,ports,1024,&count) && count == 1 && ports[0] == 65535);
    platform_net_ops_t ops = {arp,ping,cancel,clock_fake,tcp}; fake_t fake = {false,SCAN_REFUSED,0}; lan_host_t host;
    CHECK(scanner_probe(&ops,&fake,0xc0a80101,300,&host) == SCAN_OK); CHECK(host.online && fake.calls == 1 && host.port_count == 0);
    fake.result = SCAN_TIMEOUT; fake.calls = 0; CHECK(scanner_probe(&ops,&fake,1,300,&host) == SCAN_TIMEOUT); CHECK(fake.calls == discovery_port_count);
    fake.result = SCAN_CANCELLED; CHECK(scanner_probe(&ops,&fake,1,300,&host) == SCAN_CANCELLED);
    fake.result = SCAN_OK; fake.calls = 0; CHECK(scanner_probe(&ops,&fake,1,300,&host) == SCAN_OK);
    host.has_latency = true; host.ttl = 128;
    CHECK(scanner_services(&ops,&fake,&host,quick_ports,quick_port_count,250) == SCAN_OK);
    CHECK(host.ports_checked && host.port_count == 2 && host.open_ports[0] == 80 && host.open_ports[1] == 445 && host.os_family == OS_WINDOWS);
    fake.cancelled = true; CHECK(scanner_services(&ops,&fake,&host,quick_ports,quick_port_count,250) == SCAN_CANCELLED);
    const uint8_t valid[] = {'O','U','I','1',1,0,0,0,5,0,0,0,0x33,0x22,0,0,0,0,0,0,'A','c','m','e',0};
    vendor_table_t table; uint8_t mac[] = {0,0x22,0x33,1,2,3};
    CHECK(vendor_open(&table,valid,sizeof(valid))); CHECK(!strcmp(vendor_lookup(&table,mac),"Acme"));
    mac[0] = 2; CHECK(vendor_lookup(&table,mac) == NULL); mac[0] = 1; CHECK(vendor_lookup(&table,mac) == NULL);
    mac[0] = 0; mac[2] = 0x34; CHECK(vendor_lookup(&table,mac) == NULL);
    CHECK(!vendor_open(&table,valid,sizeof(valid)-1));
    uint8_t invalid[sizeof(valid)]; memcpy(invalid,valid,sizeof(valid)); invalid[16] = 5; CHECK(!vendor_open(&table,invalid,sizeof(invalid)));
    memcpy(invalid,valid,sizeof(valid)); invalid[24] = 'x'; CHECK(!vendor_open(&table,invalid,sizeof(invalid)));
    puts("Feature tests passed: port parser, TCP refused discovery, open/closed service distinction, cancellation, cautious OS hints, vendor lookup and invalid database rejection.");
    return 0;
}
