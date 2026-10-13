#include "profile.h"
#include "report.h"
#include "statistics.h"
#include "monitor.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
typedef struct { char bytes[65536]; size_t length; bool reject; } memory_t;
static profile_t profile, loaded;
static memory_t output;
static bool write_memory(void *context, const char *s, size_t n) { memory_t *m = context; if (m->reject || n >= sizeof(m->bytes)-m->length) return false; memcpy(m->bytes+m->length,s,n); m->length += n; m->bytes[m->length] = 0; return true; }
int main(void) {
    ipv4_range_t range; uint32_t ip;
    CHECK(range_parse("192.168.1.1-254",15,&range)); CHECK(range.first == 0xc0a80101 && range.last == 0xc0a801fe);
    CHECK(range_parse("10.0.0.0/24",11,&range)); CHECK(range.first == 0x0a000001 && range.last == 0x0a0000fe);
    CHECK(range_parse("10.0.0.1-10.0.0.20",18,&range)); CHECK(range.last == 0x0a000014);
    CHECK(!range_parse("10.0.0.1-0",10,&range)); CHECK(!range_parse("10.0.0.1/33",11,&range)); CHECK(!range_parse("10.0.0.1\0junk",14,&range));
    ipv4_range_t ranges[] = {{10,20},{15,30},{31,40},{100,101}}; size_t count = 4; uint64_t total;
    CHECK(ranges_normalize(ranges,&count,&total) && count == 2 && total == 33); CHECK(ranges_address_at(ranges,count,31,&ip) && ip == 100); CHECK(!ranges_contains(ranges,count,50));
    ipv4_range_t entire[] = {{0,UINT32_MAX}}; count = 1; CHECK(ranges_normalize(entire,&count,&total) && total == UINT64_C(4294967296)); CHECK(ranges_address_at(entire,count,UINT32_MAX,&ip) && ip == UINT32_MAX);
    profile_defaults(&profile); profile.ranges[0] = (ipv4_range_t){0xc0a80101,0xc0a801fe}; profile.range_count = 1;
    snprintf(profile.name,sizeof(profile.name),"Офис & Home"); snprintf(profile.notes,sizeof(profile.notes),"line one\nline two \\ tail ");
    CHECK(profile_write(&profile,write_memory,&output)); char error[256];
    CHECK(profile_parse(output.bytes,output.length,&loaded,error,sizeof(error))); CHECK(!strcmp(profile.name,loaded.name) && !strcmp(profile.notes,loaded.notes) && loaded.port_count == profile.port_count && loaded.concurrency == 64);
    const char good[] = "\xef\xbb\xbfLook@LAN-ngProfile=1\r\nname=Home\r\nrange=10.0.0.1-20\r\nrange=10.0.0.10-30\r\nports=80,443,80\r\nconcurrency=256\r\n";
    CHECK(profile_parse(good,sizeof(good)-1,&loaded,error,sizeof(error))); CHECK(loaded.range_count == 1 && loaded.ranges[0].last == 0x0a00001e && loaded.port_count == 2 && loaded.concurrency == 256);
    /* Sparse lists above the old 8 KiB formatting limit must round-trip. */
    profile_defaults(&profile);profile.ranges[0]=(ipv4_range_t){0xc0a80101,0xc0a80101};profile.range_count=1;profile.port_count=4000;
    for(size_t i=0;i<profile.port_count;++i)profile.ports[i]=(uint16_t)(i*2+1);
    memset(&output,0,sizeof(output));CHECK(profile_write(&profile,write_memory,&output) && output.length>8192);CHECK(profile_parse(output.bytes,output.length,&loaded,error,sizeof(error)) && loaded.port_count==4000 && loaded.ports[3999]==7999);
    const char bad[] = "Look@LAN-ngProfile=1\nrange=10.0.0.1\nconcurrency=7\n"; loaded.concurrency = 99;
    CHECK(!profile_parse(bad,sizeof(bad)-1,&loaded,error,sizeof(error)) && loaded.concurrency == 99);
    const char duplicate[] = "Look@LAN-ngProfile=1\nrange=10.0.0.1\nname=A\nname=B\n"; CHECK(!profile_parse(duplicate,sizeof(duplicate)-1,&loaded,error,sizeof(error)));
    const char future[] = "Look@LAN-ngProfile=2\nrange=10.0.0.1\n"; CHECK(!profile_parse(future,sizeof(future)-1,&loaded,error,sizeof(error)));
    const char invalid[] = "Look@LAN-ngProfile=1\nname=\xc0\xaf\nrange=10.0.0.1\n"; CHECK(!profile_parse(invalid,sizeof(invalid)-1,&loaded,error,sizeof(error)));
    const char unknown[] = "Look@LAN-ngProfile=1\nrange=10.0.0.1\ncloud=true\n"; CHECK(!profile_parse(unknown,sizeof(unknown)-1,&loaded,error,sizeof(error)));
    const char escape[] = "Look@LAN-ngProfile=1\nrange=10.0.0.1\nnotes=bad\\q\n"; CHECK(!profile_parse(escape,sizeof(escape)-1,&loaded,error,sizeof(error)));
    const char multiline[] = "Look@LAN-ngProfile=1\nrange=10.0.0.1\nname=bad\\nname\n"; CHECK(!profile_parse(multiline,sizeof(multiline)-1,&loaded,error,sizeof(error)));
    profile_t *bad_profile = &loaded; profile_defaults(bad_profile); bad_profile->range_count = 1; bad_profile->ranges[0] = profile.ranges[0]; bad_profile->icmp_timeout_ms = 49; CHECK(!profile_validate(bad_profile,error,sizeof(error)));
    lan_host_t hosts[3] = {0}; hosts[0].address = ipv4_address(0xc0a80101); hosts[0].first_seen = hosts[0].last_seen = 100; hosts[0].online = true; hosts[0].new_device = true; hosts[0].os_family = OS_WINDOWS; hosts[0].os_confidence = 65; hosts[0].ports_checked = true; hosts[0].port_count = 1; hosts[0].open_ports[0] = 80;
    snprintf(hosts[0].hostname,sizeof(hosts[0].hostname),"=HYPERLINK(\"bad\"),\n<script>alert(1)</script>&"); hosts[1].address = ipv4_address(0xc0a80102); hosts[1].first_seen = 99; hosts[2].address = ipv4_address(0xc0a80103);
    report_options_t options = {0}; snprintf(options.profile_name,sizeof(options.profile_name),"Office <&>"); snprintf(options.copyright,sizeof(options.copyright),"© nr084, 2026"); vendor_table_t vendors = {0};
    memset(&output,0,sizeof(output)); CHECK(report_write(REPORT_CSV,hosts,3,&vendors,&options,write_memory,&output)); CHECK(!memcmp(output.bytes,"\xef\xbb\xbf",3)); CHECK(strstr(output.bytes,"\"'=HYPERLINK(\"\"bad\"\")") && strstr(output.bytes,"OFFLINE") && strstr(output.bytes,"UNKNOWN"));
    memset(&output,0,sizeof(output)); CHECK(report_write(REPORT_HTML,hosts,3,&vendors,&options,write_memory,&output)); CHECK(strstr(output.bytes,"&lt;script&gt;") && !strstr(output.bytes,"<script>") && strstr(output.bytes,"Office &lt;&amp;&gt;") && strstr(output.bytes,"</html>"));
    memset(&output,0,sizeof(output)); options.russian = true; CHECK(report_write(REPORT_TXT,hosts,3,&vendors,&options,write_memory,&output)); CHECK(strstr(output.bytes,"Статус") && strstr(output.bytes,"1970-01-01 00:01:40"));
    output.reject = true; CHECK(!report_write(REPORT_HTML,hosts,3,&vendors,&options,write_memory,&output));
    statistics_t stats = statistics_compute(hosts,3,254); CHECK(stats.records == 3 && stats.online == 1 && stats.offline == 1 && stats.unknown == 1 && stats.fresh == 1 && stats.windows == 1 && stats.scanned == 254);
    hosts[1].cache_only=true;stats=statistics_compute(hosts,3,254);CHECK(stats.unknown==2 && stats.offline==0);memset(&output,0,sizeof(output));CHECK(report_write(REPORT_CSV,hosts,3,&vendors,&options,write_memory,&output));CHECK(!strstr(output.bytes,"OFFLINE"));hosts[1].cache_only=false;
    lan_host_t tracked[4]; size_t tracked_count; monitor_event_t log[32]; monitor_t monitor;
    monitor_init(&monitor,tracked,&tracked_count,4,log,32); hosts[0].interface_index = 1; hosts[1].interface_index = 1; hosts[1].online = true;
    ipv4_range_t all = {0xc0a80101,0xc0a80103}; monitor_begin(&monitor,all,1,true,200); monitor_observe(&monitor,&hosts[0],true,201); monitor_observe(&monitor,&hosts[1],true,201); monitor_finish(&monitor,true,202);
    ipv4_range_t scopes[] = {{0xc0a80101,0xc0a80101},{0xc0a80103,0xc0a80103}};
    for (unsigned i = 0; i < 2; ++i) { monitor_begin_ranges(&monitor,scopes,2,1,true,210+i*10); monitor_observe(&monitor,&hosts[0],true,211+i*10); monitor_finish(&monitor,true,212+i*10); }
    CHECK(tracked[1].online && tracked[1].missed_rounds == 0);
    puts("Profile/report tests passed: bounded ranges and union, UTF-8 versioned round-trip, malformed profile rejection without mutation, CSV/HTML/TXT escaping, writer failure, stats and disjoint monitoring scope.");
    return 0;
}
