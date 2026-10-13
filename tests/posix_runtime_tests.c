#include "desktop_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "posix_names.h"
#include "statistics.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
typedef struct {int socket;uint32_t expected;bool source_ok;} dns_fixture_t;
static size_t dns_answer(uint32_t ip,uint16_t tx,uint8_t packet[512]){
    size_t n=posix_ptr_request(ip,tx,packet);packet[2]=0x81;packet[3]=0x80;packet[7]=1;
    const uint8_t tail[]={0xc0,0x0c,0,12,0,1,0,0,0,60,0,14,4,'t','e','s','t',7,'e','x','a','m','p','l','e',0};memcpy(packet+n,tail,sizeof(tail));return n+sizeof(tail);
}
static void *dns_server(void *context){dns_fixture_t *f=context;uint8_t packet[512];struct sockaddr_in peer={0};socklen_t size=sizeof(peer);ssize_t bytes=recvfrom(f->socket,packet,sizeof(packet),0,(struct sockaddr *)&peer,&size);if(bytes>12){f->source_ok=ntohl(peer.sin_addr.s_addr)==f->expected;uint16_t tx=(packet[0]<<8)|packet[1];size_t n=dns_answer(f->expected,tx,packet);sendto(f->socket,packet,n,0,(struct sockaddr *)&peer,size);}return NULL;}
static size_t resident_kib(void){
#ifdef __linux__
    FILE *file=fopen("/proc/self/status","r");if(!file)return 0;char line[256];size_t value=0;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"VmRSS: %zu kB",&value)==1)break;
    fclose(file);return value;
#else
    return 0;
#endif
}
static int monitor_stress(void){
    desktop_app_t *a=calloc(1,sizeof(*a));CHECK(a && desktop_init(a,"en_US"));CHECK(a->selected<a->interface_count);
    char ranges[96],own[32],gateway[32];ipv4_format(a->interfaces[a->selected].ip,own,sizeof(own));uint32_t router=a->interfaces[a->selected].gateway;
    if(router){ipv4_format(router,gateway,sizeof(gateway));snprintf(ranges,sizeof(ranges),"%s;%s",own,gateway);}else snprintf(ranges,sizeof(ranges),"%s",own);
    CHECK(desktop_profile_configure(a,"Monitoring stress",ranges,"22",100,100,8,5,""));CHECK(desktop_monitor(a,NULL));
    uint64_t deadline=now()+720000,last_round=0;int baseline_fd=-1;size_t baseline_rss=0,max_rss=0;unsigned samples=0;
    while(a->monitoring && a->monitor.round<121 && now()<deadline){
        desktop_poll(a);if(!a->scanning && !a->names_pending && a->monitor.round!=last_round){last_round=a->monitor.round;int fd=descriptors();size_t rss=resident_kib();
            if(last_round==5){baseline_fd=fd;baseline_rss=rss;}if(last_round>=5){CHECK(fd<=baseline_fd);if(rss>max_rss)max_rss=rss;++samples;}
            printf("MONITOR round=%llu hosts=%zu fd=%d rss_kib=%zu\n",(unsigned long long)last_round,a->host_count,fd,rss);fflush(stdout);
        }struct timespec pause={0,50000000};nanosleep(&pause,NULL);
    }
    CHECK(a->monitor.round>=121 && a->monitoring && samples>=100);desktop_stop(a);CHECK(finish_scan(a));CHECK(!baseline_rss || max_rss<=baseline_rss+4096);
    while(!desktop_close(a))pause_tick();free(a);printf("PASS 120 scheduled monitoring rounds (~10 minutes), stable FDs, RSS %zu -> max %zu KiB\n",baseline_rss,max_rss);return 0;
}
int main(int argc,char **argv){
    if(argc==2 && !strcmp(argv[1],"--monitor-stress"))return monitor_stress();
    const char *fixture_interface=getenv("LOOKLAN_ARP_INTERFACE"),*fixture_target=getenv("LOOKLAN_ARP_TARGET");
    if(fixture_interface && fixture_target){desktop_app_t *fixture=calloc(1,sizeof(*fixture));CHECK(fixture && desktop_init(fixture,"en_US"));size_t selected=SIZE_MAX;for(size_t i=0;i<fixture->interface_count;++i)if(!strcmp(fixture->interfaces[i].name,fixture_interface))selected=i;CHECK(selected<fixture->interface_count && desktop_select(fixture,selected));CHECK(desktop_profile_configure(fixture,"ARP fixture",fixture_target,"22",100,100,8,5,""));CHECK(desktop_start(fixture,fixture_target) && finish_scan(fixture));uint32_t ip;CHECK(ipv4_parse(fixture_target,strlen(fixture_target),&ip));size_t found=SIZE_MAX;for(size_t i=0;i<fixture->host_count;++i)if(ipv4_value(&fixture->hosts[i].address)==ip)found=i;CHECK(found<fixture->host_count);bool cached=getenv("LOOKLAN_ARP_CACHE")!=NULL;
        if(cached)CHECK(!fixture->hosts[found].online && !fixture->hosts[found].first_seen && fixture->hosts[found].has_mac);else CHECK(fixture->hosts[found].online && fixture->hosts[found].has_mac && !fixture->hosts[found].has_latency && fixture->arp_only[found]);CHECK(getuid()!=0);while(!desktop_close(fixture))pause_tick();free(fixture);printf("PASS unprivileged %s fixture: all ICMP/TCP/UDP input blocked, ARP alone %s\n",cached ? "cached neighbor":"fresh ARP",cached ? "does not prove ONLINE":"confirms ONLINE");return 0;
    }
    desktop_app_t *a=calloc(1,sizeof(*a));CHECK(a && desktop_init(a,"en_US"));
    if(!a->interface_count){desktop_close(a);free(a);return 77;}
    char ip[32];ipv4_format(a->interfaces[a->selected].ip,ip,sizeof(ip));
    CHECK(desktop_start(a,ip) && finish_scan(a));CHECK(a->host_count==1 && a->hosts[0].online);
    CHECK(ipv4_value(&a->hosts[0].address)==a->interfaces[a->selected].ip);
    CHECK(!desktop_start(a,"garbage"));CHECK(desktop_start(a,ip) && finish_scan(a));
    CHECK(a->host_count==1 && !a->error[0]);
    uint8_t packet[512];char name[256];uint32_t own=a->interfaces[a->selected].ip;size_t bytes=dns_answer(own,123,packet);
    CHECK(posix_ptr_reply(packet,bytes,own,123,name) && !strcmp(name,"test.example"));CHECK(!posix_ptr_reply(packet,bytes,own,124,name));CHECK(!posix_ptr_reply(packet,bytes-1,own,123,name));
    packet[12]=0xc0;packet[13]=12;CHECK(!posix_ptr_reply(packet,bytes,own,123,name));
    bytes=posix_ptr_request(own,123,packet);packet[2]=0x81;packet[3]=0x80;packet[7]=2;
    const uint8_t alias_records[]={0xc0,0x0c,0,5,0,1,0,0,0,60,0,15,5,'a','l','i','a','s',7,'r','e','v','e','r','s','e',0,5,'a','l','i','a','s',7,'r','e','v','e','r','s','e',0,0,12,0,1,0,0,0,60,0,14,4,'t','e','s','t',7,'e','x','a','m','p','l','e',0};
    memcpy(packet+bytes,alias_records,sizeof(alias_records));CHECK(posix_ptr_reply(packet,bytes+sizeof(alias_records),own,123,name) && !strcmp(name,"test.example"));CHECK(!posix_ptr_reply(packet,bytes+sizeof(alias_records)-1,own,123,name));
    uint8_t answer[512];size_t used=posix_address_request("fixture.example",42,answer);CHECK(used);answer[2]=0x81;answer[3]=0x80;answer[7]=1;const uint8_t a_record[]={0xc0,0x0c,0,1,0,1,0,0,0,60,0,4,192,0,2,7};memcpy(answer+used,a_record,sizeof(a_record));uint32_t resolved=0;CHECK(posix_address_reply(answer,used+sizeof(a_record),"fixture.example",42,&resolved) && resolved==0xc0000207);CHECK(!posix_address_reply(answer,used+sizeof(a_record)-1,"fixture.example",42,&resolved));CHECK(!posix_address_request("name;command",1,answer));CHECK(!posix_address_request("a..b",1,answer));
    int server=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);CHECK(server>=0);struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(own);CHECK(!bind(server,(struct sockaddr *)&address,sizeof(address)));socklen_t size=sizeof(address);CHECK(!getsockname(server,(struct sockaddr *)&address,&size));
    struct timeval receive_timeout={2,0};setsockopt(server,SOL_SOCKET,SO_RCVTIMEO,&receive_timeout,sizeof(receive_timeout));dns_fixture_t fixture={server,own,false};pthread_t thread;CHECK(!pthread_create(&thread,NULL,dns_server,&fixture));
    CHECK(posix_ptr_lookup(&a->backend,own,own,ntohs(address.sin_port),500,name));pthread_join(thread,NULL);CHECK(fixture.source_ok && !strcmp(name,"test.example"));close(server);
    CHECK(desktop_profile_configure(a,"Native profile",ip,"22,80",100,100,8,5,"UTF-8 test"));profile_t before_profile=a->profile;
    CHECK(!desktop_profile_configure(a,"Invalid",ip,"0",100,100,8,5,""));CHECK(!memcmp(&before_profile,&a->profile,sizeof(before_profile)));
    CHECK(desktop_monitor(a,ip));CHECK(a->monitoring && !desktop_select(a,a->selected));CHECK(finish_scan(a));uint64_t round=a->monitor.round,deadline=now()+7000;
    while(a->monitor.round==round && now()<deadline){desktop_poll(a);pause_tick();}CHECK(a->monitor.round==round+1 && finish_scan(a));desktop_stop(a);CHECK(!a->monitoring);
    char history[8192];desktop_history(a,history,sizeof(history));CHECK(strstr(history,"Scan finished"));CHECK(statistics_compute(a->hosts,a->host_count,a->completed).online==1);
    CHECK(details_note_set(a->details,own,a->interfaces[a->selected].index,"Device note"));CHECK(!strcmp(details_note(a->details,own,a->interfaces[a->selected].index),"Device note"));
    size_t ping_before=a->details->ping_count;CHECK(desktop_diagnostic(a,own,false));deadline=now()+6000;while(a->tools && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->tools && a->details->ping_count==ping_before+4);for(size_t i=ping_before;i<ping_before+4;++i)CHECK(details_ping_at(a->details,i)->received);
    CHECK(desktop_diagnostic(a,own,true));deadline=now()+6000;while(a->tools && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->tools && a->trace_count==1 && a->trace[0].received && a->trace[0].reply_ip==own);
    CHECK(desktop_diagnostic(a,own,false));uint64_t cancel_began=now();desktop_stop(a);while(a->tools && now()-cancel_began<2000){desktop_poll(a);pause_tick();}CHECK(!a->tools);
    char directory[]="/tmp/looklan-native-XXXXXX";CHECK(mkdtemp(directory));char path[512];snprintf(path,sizeof(path),"%s/export.csv",directory);
    strcpy(a->hosts[0].hostname,"snapshot-name");CHECK(desktop_export(a,path,REPORT_CSV));strcpy(a->hosts[0].hostname,"changed-after-start");deadline=now()+10000;while(a->io && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->io);
    FILE *file=fopen(path,"rb");CHECK(file);char contents[8192];size_t read=fread(contents,1,sizeof(contents)-1,file);contents[read]=0;fclose(file);CHECK(strstr(contents,"snapshot-name") && !strstr(contents,"changed-after-start"));unlink(path);
    snprintf(path,sizeof(path),"%s/профиль.lanprofile",directory);CHECK(desktop_profile_save(a,path));deadline=now()+10000;while(a->io && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->io);CHECK(desktop_profile_open(a,path));deadline=now()+10000;while(a->io && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->io && !strcmp(a->profile.name,"Native profile"));unlink(path);
    lan_host_t *many=calloc(65536,sizeof(*many));CHECK(many);for(size_t i=0;i<65536;++i){many[i].address=ipv4_address((uint32_t)i);strcpy(many[i].hostname,"cancel-fixture");}
    snprintf(path,sizeof(path),"%s/cancel.html",directory);file=fopen(path,"wb");CHECK(file);fputs("original",file);fclose(file);desktop_io_t *io=desktop_io_start(DESKTOP_EXPORT,path,&a->profile,many,65536,&a->vendors,REPORT_HTML,false,NULL);CHECK(io);desktop_io_cancel(io);deadline=now()+10000;while(!desktop_io_finished(io) && now()<deadline)pause_tick();CHECK(desktop_io_finished(io));bool cancelled=false;bool committed=desktop_io_result(io,NULL,&cancelled);CHECK(cancelled);desktop_io_free(io);free(many);file=fopen(path,"rb");CHECK(file);read=fread(contents,1,sizeof(contents)-1,file);contents[read]=0;fclose(file);CHECK(committed || !strcmp(contents,"original"));unlink(path);rmdir(directory);
    CHECK(desktop_start(a,ip) && finish_scan(a));
    size_t range_count=a->profile.range_count;ipv4_range_t saved_range=a->profile.ranges[0];CHECK(desktop_single_target(a,"localhost"));deadline=now()+6000;while((a->tools || a->scanning) && now()<deadline){desktop_poll(a);pause_tick();}CHECK(!a->tools && !a->scanning && a->resolved_ip==0x7f000001);CHECK(a->profile.range_count==range_count && !memcmp(&saved_range,&a->profile.ranges[0],sizeof(saved_range)));
    /* Cache is interface/source/MAC-scoped; stale generations cannot publish. */
    posix_backend_t *cache=calloc(1,sizeof(*cache));CHECK(cache && posix_backend_init(cache));cache->selected=a->interfaces[a->selected];cache->profile=a->profile;cache->profile.ranges[0]=(ipv4_range_t){own,own};cache->profile.range_count=1;cache->provider_count=1;
    cache->name_cache[0]=(posix_name_cache_t){.ip=own,.interface_index=cache->selected.index,.source=cache->selected.ip,.expires=now()+60000,.has_mac=true};strcpy(cache->name_cache[0].name,"cached.example");memcpy(cache->name_cache[0].mac,cache->selected.mac,6);memcpy(cache->neighbor_mac[0],cache->selected.mac,6);posix_queue_name(cache,own);posix_event_t cached_event;CHECK(posix_pop(cache,&cached_event) && cached_event.type==PX_NAME && !strcmp(cached_event.host.hostname,"cached.example") && !cache->name_count);
    cache->name_queued[0]=false;cache->neighbor_mac[0][5]^=1;posix_queue_name(cache,own);CHECK(cache->name_count==1 && !posix_pop(cache,&cached_event));uint64_t old_generation=atomic_load(&cache->generation);posix_invalidate_names(cache);CHECK(!cache->name_count && !posix_publish(cache,(posix_event_t){.type=PX_NAME,.generation=old_generation}));cache->provider_count=0;CHECK(posix_backend_destroy(cache));free(cache);
    profile_t original=a->profile;a->pending_profile=malloc(sizeof(*a->pending_profile));CHECK(a->pending_profile);*a->pending_profile=original;strcpy(a->pending_profile->interface_id,"missing-test-interface");CHECK(!strcmp(a->profile.interface_id,original.interface_id));CHECK(desktop_profile_rebind(a,a->selected) && !a->pending_profile && !strcmp(a->profile.interface_id,a->interfaces[a->selected].name));
    CHECK(desktop_start(a,ip) && finish_scan(a));posix_interface_t real_interface=a->interfaces[a->selected];strcpy(a->interfaces[a->selected].name,"missing-test-interface");CHECK(desktop_start(a,ip) && finish_scan(a) && a->failed && a->hosts[0].online);a->interfaces[a->selected]=real_interface;
    int before=descriptors();uint64_t max_stop=0;
    for(unsigned i=0;i<100;++i){
        CHECK(desktop_start(a,ip));uint64_t began=now();posix_stop(&a->backend);
        CHECK(finish_scan(a));uint64_t elapsed=now()-began;if(elapsed>max_stop)max_stop=elapsed;
        CHECK(!a->backend.joinable && !atomic_load(&a->backend.running));
    }
    int after=descriptors();CHECK(before<0 || after<=before);CHECK(max_stop<2000);
    CHECK(desktop_start(a,ip));uint64_t began=now();while(!desktop_close(a) && now()-began<3000)pause_tick();
    CHECK(a->closed && now()-began<3000 && desktop_close(a));
    printf("PASS POSIX: scoped name cache/MAC invalidation/stale generations, missing-interface protection/remapping, DNS parser/source-bound UDP fixture, profile editor, timed monitoring, history/statistics/notes, 4 Ping replies, 1-hop Trace, diagnostic cancellation, immutable async export, UTF-8 async profile roundtrip, cancelled export preserves target, 100 scan/Stop cycles, FD %d -> %d, max Stop %llu ms, close/idempotence\n",before,after,(unsigned long long)max_stop);
    free(a);return 0;
}
