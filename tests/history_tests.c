#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static lan_host_t hosts[16],restored[16];static monitor_event_t events[8],restored_events[8];static profile_t p;static unsigned char bytes[20000];static size_t length;
static bool write_memory(void *ctx,const char *b,size_t n){(void)ctx;if(n>sizeof(bytes)-length)return false;memcpy(bytes+length,b,n);length+=n;return true;}
int main(void){
    size_t count=0,restored_count=0;monitor_t m,r;profile_defaults(&p);strcpy(p.interface_id,"test0");p.ranges[0]=(ipv4_range_t){0x0a000001,0x0a0000fe};p.range_count=1;
    monitor_init(&m,hosts,&count,16,events,8);monitor_init(&r,restored,&restored_count,16,restored_events,8);
    lan_host_t h={0};h.address=ipv4_address(0x0a000001);h.interface_index=7;h.has_mac=true;h.mac[0]=0x50;h.online=true;strcpy(h.hostname,"router");h.ports_checked=true;h.port_count=1;h.open_ports[0]=443;
    monitor_begin(&m,p.ranges[0],7,true,100);CHECK(monitor_observe(&m,&h,true,101)==0);monitor_finish(&m,true,102);
    for(unsigned i=0;i<12;++i)monitor_note(&m,MON_SCAN_ERROR,"test",110+i);
    memset(p.interface_id+6,0x7f,sizeof(p.interface_id)-6); /* stale tail after shortening/rebinding an interface ID */
    CHECK(history_write(&m,&p,write_memory,NULL));memset(p.interface_id+6,0,sizeof(p.interface_id)-6);
    CHECK(history_read(bytes,length,&r,&p,99));CHECK(restored_count==1 && r.log_count==8 && r.sequence==m.sequence && r.baseline);
    CHECK(restored[0].historical && restored[0].online && restored[0].interface_index==99 && restored[0].first_seen==101 && restored[0].open_ports[0]==443);
    CHECK(monitor_log_at(&r,0)->sequence==monitor_log_at(&m,0)->sequence);
    for(size_t i=0;i<length;i+=17){unsigned char v=bytes[i];bytes[i]^=1;CHECK(!history_read(bytes,length,&r,&p,99));CHECK(restored_count==1 && restored[0].first_seen==101);bytes[i]=v;}
    CHECK(!history_read(bytes,length-1,&r,&p,99));CHECK(!history_read(bytes,length+1,&r,&p,99));strcpy(p.interface_id,"other");CHECK(!history_read(bytes,length,&r,&p,99));strcpy(p.interface_id,"test0");
    monitor_begin(&r,p.ranges[0],99,true,200);h.interface_index=99;h.mac[5]=1;strcpy(h.hostname,"renamed");CHECK(monitor_observe(&r,&h,true,201)==0);monitor_finish(&r,true,202);
    CHECK(!restored[0].historical && restored[0].changed && restored[0].first_seen==101 && restored[0].last_seen==201);
    monitor_begin(&r,p.ranges[0],99,true,300);monitor_finish(&r,true,301);CHECK(restored[0].online);monitor_begin(&r,p.ranges[0],99,true,302);monitor_finish(&r,true,303);CHECK(!restored[0].online);
    length=0;CHECK(history_write(&r,&p,write_memory,NULL));CHECK(history_read(bytes,length,&m,&p,7));CHECK(!hosts[0].online && hosts[0].historical && hosts[0].last_seen==201);
    puts("PASS: history round trip, ring, CRC/truncation, foreign profile, NIC rebind and monitoring across restarts");return 0;
}
