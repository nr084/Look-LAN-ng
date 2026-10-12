#include "details.h"
#include "nbns.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Details/NBNS check failed at %d: %s\n",__LINE__,#x); return 1; } } while (0)
static details_store_t store;
int main(void) {
    CHECK(details_note_set(&store,1,2,"Printer") && details_note_set(&store,1,3,"Other NIC"));
    CHECK(!strcmp(details_note(&store,1,2),"Printer") && !strcmp(details_note(&store,1,3),"Other NIC"));
    char oversized[513]; memset(oversized,'x',512); oversized[512] = 0;
    CHECK(!details_note_set(&store,1,2,oversized) && !strcmp(details_note(&store,1,2),"Printer"));
    CHECK(details_note_set(&store,1,2,"") && !details_note(&store,1,2)[0] && store.note_count == 1);
    details_reset(&store);
    for (unsigned i = 0; i < DEVICE_NOTES_CAPACITY; ++i) CHECK(details_note_set(&store,i,1,"Device"));
    CHECK(!details_note_set(&store,99999,1,"Full") && details_note_set(&store,2,1,"") && details_note_set(&store,99999,1,"Free slot"));
    details_reset(&store);
    for (unsigned i = 0; i < PING_HISTORY_CAPACITY+5; ++i) { ping_sample_t sample = {0}; sample.ip = i; sample.interface_index = 2; details_ping_add(&store,sample); }
    CHECK(store.ping_count == PING_HISTORY_CAPACITY && details_ping_at(&store,0)->ip == 5 && !details_ping_at(&store,PING_HISTORY_CAPACITY));
    uint8_t packet[128] = {0}, request[50]; char name[16] = "preserved";
    CHECK(nbns_request(0x1234,request,sizeof(request)) && !nbns_request(1,request,49) && request[13] == 'C' && request[14] == 'K');
    memcpy(packet,request,50); packet[2] = 0x84; packet[7] = 1;
    packet[50] = 0xc0; packet[51] = 12; packet[53] = 0x21; packet[55] = 1; packet[61] = 37; packet[62] = 2;
    memset(packet+63,' ',15); memcpy(packet+63,"WORKGROUP",9); packet[79] = 0x80;
    memset(packet+81,' ',15); memcpy(packet+81,"WORKSTATION",11); packet[97] = 4;
    CHECK(nbns_name(packet,99,0x1234,name) && !strcmp(name,"WORKSTATION"));
    for (size_t length = 0; length < 99; ++length) CHECK(!nbns_name(packet,length,0x1234,name));
    CHECK(!nbns_name(packet,99,0x4321,name));
    packet[2] = 0x86; CHECK(!nbns_name(packet,99,0x1234,name)); packet[2] = 0x84;
    packet[51] = 200; CHECK(!nbns_name(packet,99,0x1234,name)); packet[51] = 12;
    packet[98] = 0; packet[97] = 0x80; CHECK(!nbns_name(packet,99,0x1234,name)); packet[97] = 4;
    packet[81] = 10; CHECK(!nbns_name(packet,99,0x1234,name)); packet[81] = 'W';
    packet[62] = 255; CHECK(!nbns_name(packet,99,0x1234,name));
    puts("Details/NBNS passed: scoped notes/capacity/clear, ping ring eviction, wildcard encoding, workstation selection, truncated/wrong transaction/group/control/malformed replies."); return 0;
}
