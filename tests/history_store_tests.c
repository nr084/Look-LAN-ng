#include "../src/platform/history_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static profile_t profile;
static void pause_tick(void){
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t={0,1000000};nanosleep(&t,NULL);
#endif
}
static bool finish(history_store_t *j){if(!j)return false;for(unsigned i=0;i<10000 && !history_store_done(j);++i)pause_tick();return history_store_done(j);}
int main(void){
    const char *path="history-store-test.lanhistory";lan_host_t hosts[4]={0},restored[4]={0};monitor_event_t events[16],loaded_events[16];size_t count=0,n=0;monitor_t m,r;
    remove(path);profile_defaults(&profile);strcpy(profile.interface_id,"nic");profile.range_count=1;profile.ranges[0]=(ipv4_range_t){0x7f000001,0x7f000001};
    monitor_init(&m,hosts,&count,4,events,16);monitor_init(&r,restored,&n,4,loaded_events,16);
    lan_host_t h={0};h.address=ipv4_address(0x7f000001);h.interface_index=1;strcpy(h.hostname,"snapshot");monitor_begin(&m,profile.ranges[0],1,true,100);monitor_observe(&m,&h,true,101);monitor_finish(&m,true,102);
    history_store_t *job=history_store_start(path,&profile,&m,false,0);strcpy(hosts[0].hostname,"changed after snapshot");CHECK(finish(job) && history_store_success(job));history_store_free(job);CHECK(history_store_exists(path));
    job=history_store_start(path,&profile,&r,true,42);CHECK(finish(job) && history_store_success(job));history_store_apply(job,&r);history_store_free(job);CHECK(n==1 && restored[0].historical && restored[0].interface_index==42 && !strcmp(restored[0].hostname,"snapshot"));
    strcpy(hosts[0].hostname,"replacement");job=history_store_start(path,&profile,&m,false,0);CHECK(finish(job) && history_store_success(job));history_store_free(job);
    job=history_store_start(path,&profile,&r,true,42);CHECK(finish(job) && history_store_success(job));history_store_apply(job,&r);history_store_free(job);CHECK(!strcmp(restored[0].hostname,"replacement"));
    FILE *f=fopen(path,"wb");CHECK(f);CHECK(fputs("truncated\n",f)>=0);fclose(f);
    job=history_store_start(path,&profile,&r,true,42);CHECK(finish(job) && !history_store_success(job));history_store_apply(job,&r);history_store_free(job);CHECK(n==1 && !strcmp(restored[0].hostname,"replacement"));
    CHECK(remove(path)==0);puts("PASS: asynchronous immutable snapshot, atomic replacement, restart, corruption preserves live model");return 0;
}
