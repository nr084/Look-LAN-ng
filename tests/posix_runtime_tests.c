#include "desktop_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <dirent.h>
#endif
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} } while(0)
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void pause_tick(void){struct timespec t={0,5000000};nanosleep(&t,NULL);}
static bool finish_scan(desktop_app_t *a){uint64_t deadline=now()+10000;while(a->scanning && now()<deadline){desktop_poll(a);pause_tick();}return !a->scanning;}
static int descriptors(void){
#ifdef __linux__
    DIR *d=opendir("/proc/self/fd");if(!d)return -1;int n=0;while(readdir(d))++n;closedir(d);return n;
#else
    return -1;
#endif
}
int main(void){
    desktop_app_t *a=calloc(1,sizeof(*a));CHECK(a && desktop_init(a,"en_US"));
    if(!a->interface_count){desktop_close(a);free(a);return 77;}
    char ip[32];ipv4_format(a->interfaces[a->selected].ip,ip,sizeof(ip));
    CHECK(desktop_start(a,ip) && finish_scan(a));CHECK(a->host_count==1 && a->hosts[0].online);
    CHECK(ipv4_value(&a->hosts[0].address)==a->interfaces[a->selected].ip);
    CHECK(!desktop_start(a,"garbage"));CHECK(desktop_start(a,ip) && finish_scan(a));
    CHECK(a->host_count==1 && !a->error[0]);
    int before=descriptors();uint64_t max_stop=0;
    for(unsigned i=0;i<100;++i){
        CHECK(desktop_start(a,ip));uint64_t began=now();posix_stop(&a->backend);
        CHECK(finish_scan(a));uint64_t elapsed=now()-began;if(elapsed>max_stop)max_stop=elapsed;
        CHECK(!a->backend.joinable && !atomic_load(&a->backend.running));
    }
    int after=descriptors();CHECK(before<0 || after<=before);CHECK(max_stop<2000);
    CHECK(desktop_start(a,ip));uint64_t began=now();while(!desktop_close(a) && now()-began<3000)pause_tick();
    CHECK(a->closed && now()-began<3000 && desktop_close(a));
    printf("PASS POSIX: own-IP scan/rescan, invalid-range recovery, 100 scan/Stop cycles, FD %d -> %d, max Stop %llu ms, close/idempotence\n",before,after,(unsigned long long)max_stop);
    free(a);return 0;
}
