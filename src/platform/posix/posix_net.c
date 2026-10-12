#include "posix_net.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#ifdef __APPLE__
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sysctl.h>
#endif
static void cached_mac(const posix_interface_t *nic,uint32_t ip,lan_host_t *host) {
#ifdef __APPLE__
    int mib[]={CTL_NET,PF_ROUTE,0,AF_INET,NET_RT_FLAGS,RTF_LLINFO};size_t size=0;
    if(sysctl(mib,6,NULL,&size,NULL,0) || !size || size>1024*1024)return;
    char *data=malloc(size);if(!data)return;if(sysctl(mib,6,data,&size,NULL,0)){free(data);return;}
    for(size_t at=0;at+sizeof(struct rt_msghdr)<=size;) {
        struct rt_msghdr *r=(struct rt_msghdr *)(data+at);size_t length=r->rtm_msglen;
        if(length<sizeof(*r) || length>size-at)break;
        if(r->rtm_index==nic->index){size_t offset=sizeof(*r);struct sockaddr_in *destination=NULL;struct sockaddr_dl *link=NULL;
            for(unsigned i=0;i<RTAX_MAX;++i)if(r->rtm_addrs&(1<<i)){
                if(offset+2>length)break;struct sockaddr *sa=(struct sockaddr *)(data+at+offset);
                size_t bytes=sa->sa_len ? (sa->sa_len+sizeof(long)-1)&~(sizeof(long)-1) : sizeof(long);
                if(bytes>length-offset)break;
                if(i==RTAX_DST && sa->sa_family==AF_INET && sa->sa_len>=sizeof(struct sockaddr_in))destination=(struct sockaddr_in *)sa;
                if(i==RTAX_GATEWAY && sa->sa_family==AF_LINK && sa->sa_len>=offsetof(struct sockaddr_dl,sdl_data))link=(struct sockaddr_dl *)sa;
                offset+=bytes;
            }
            if(destination && ntohl(destination->sin_addr.s_addr)==ip && link && link->sdl_alen==6 && offsetof(struct sockaddr_dl,sdl_data)+link->sdl_nlen+6<=link->sdl_len){memcpy(host->mac,LLADDR(link),6);host->has_mac=true;break;}
        }at+=length;
    }free(data);
#else
    FILE *f=fopen("/proc/net/arp","r");if(!f)return;char line[512];
    while(fgets(line,sizeof(line),f)){char address[64],mac[32],device[128];unsigned type,flags,m[6];uint32_t found;
        if(sscanf(line,"%63s %x %x %31s %*s %127s",address,&type,&flags,mac,device)!=5 || !(flags&2) || strcmp(device,nic->name) || !ipv4_parse(address,strlen(address),&found) || found!=ip)continue;
        if(sscanf(mac,"%x:%x:%x:%x:%x:%x",&m[0],&m[1],&m[2],&m[3],&m[4],&m[5])==6){bool valid=true;for(unsigned i=0;i<6;++i)if(m[i]>255)valid=false;if(valid){for(unsigned i=0;i<6;++i)host->mac[i]=(uint8_t)m[i];host->has_mac=true;}}break;
    }fclose(f);
#endif
}
static uint64_t milliseconds(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static void pause_ms(unsigned ms) { struct timespec t={ms/1000,(long)(ms%1000)*1000000}; while(nanosleep(&t,&t) && errno==EINTR) {} }
size_t posix_interfaces(posix_interface_t *out,size_t capacity) {
    struct ifaddrs *list=NULL; if(getifaddrs(&list)) return 0; size_t count=0;
    for(struct ifaddrs *it=list;it && count<capacity;it=it->ifa_next) {
        if(!it->ifa_addr || it->ifa_addr->sa_family!=AF_INET || !(it->ifa_flags&IFF_UP) || (it->ifa_flags&IFF_LOOPBACK) || !it->ifa_netmask) continue;
        posix_interface_t *nic=&out[count++]; memset(nic,0,sizeof(*nic));
        snprintf(nic->name,sizeof(nic->name),"%s",it->ifa_name); nic->index=if_nametoindex(it->ifa_name);
        nic->ip=ntohl(((struct sockaddr_in *)it->ifa_addr)->sin_addr.s_addr);
        uint32_t mask=ntohl(((struct sockaddr_in *)it->ifa_netmask)->sin_addr.s_addr);
        while(mask&0x80000000u) { ++nic->prefix; mask<<=1; }
#ifdef __APPLE__
        for(struct ifaddrs *link=list;link;link=link->ifa_next) if(link->ifa_addr && link->ifa_addr->sa_family==AF_LINK && !strcmp(link->ifa_name,it->ifa_name)) {
            struct sockaddr_dl *dl=(struct sockaddr_dl *)link->ifa_addr;
            if(dl->sdl_alen==6) { memcpy(nic->mac,LLADDR(dl),6); nic->has_mac=true; } break;
        }
#else
        int fd=socket(AF_INET,SOCK_DGRAM,0); if(fd>=0) {
            struct ifreq request={0}; snprintf(request.ifr_name,sizeof(request.ifr_name),"%s",it->ifa_name);
            if(!ioctl(fd,SIOCGIFHWADDR,&request)) { memcpy(nic->mac,request.ifr_hwaddr.sa_data,6); nic->has_mac=true; } close(fd);
        }
#endif
    }
    freeifaddrs(list); return count;
}
static bool cancelled(void *context) { return atomic_load(&((posix_backend_t *)context)->cancel); }
static uint64_t wall(void *context) { (void)context; return (uint64_t)time(NULL); }
static bool emit(posix_backend_t *b,posix_event_t event) {
    for(;;) {
        pthread_mutex_lock(&b->lock);
        if(b->count<POSIX_EVENTS) { b->events[(b->head+b->count)%POSIX_EVENTS]=event; ++b->count; pthread_mutex_unlock(&b->lock); return true; }
        pthread_mutex_unlock(&b->lock); if(cancelled(b)) return false; pause_ms(10);
    }
}
static int wait_socket(posix_backend_t *b,int fd,short flags,uint64_t deadline) {
    while(!cancelled(b)) {
        uint64_t now=milliseconds(); if(now>=deadline) return 0;
        int delay=(int)(deadline-now); if(delay>25) delay=25;
        struct pollfd p={fd,flags,0}; int result=poll(&p,1,delay);
        if(result>0) return 1; if(result<0 && errno!=EINTR) return -1;
    }
    return -2;
}
static int source_socket(posix_backend_t *b,int type,int protocol) {
    int fd=socket(AF_INET,type,protocol); if(fd<0) return -1;
    struct sockaddr_in source={0}; source.sin_family=AF_INET; source.sin_addr.s_addr=htonl(b->selected.ip);
    if(bind(fd,(struct sockaddr *)&source,sizeof(source)) || fcntl(fd,F_SETFL,O_NONBLOCK)<0 || fcntl(fd,F_SETFD,FD_CLOEXEC)<0) { close(fd); return -1; }
#ifdef __APPLE__
    unsigned index=b->selected.index;
    if(setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index))) { close(fd); return -1; }
    int no_signal=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&no_signal,sizeof(no_signal));
#endif
    return fd;
}
static bool paced(posix_backend_t *b) {
    pthread_mutex_lock(&b->lock); uint64_t now=milliseconds(); if(b->ticket<now) b->ticket=now; uint64_t due=b->ticket; b->ticket+=4; pthread_mutex_unlock(&b->lock);
    while(!cancelled(b) && milliseconds()<due) pause_ms(1);
    return !cancelled(b);
}
static scan_result_t tcp(void *context,uint32_t ip,uint16_t port,uint32_t timeout) {
    posix_backend_t *b=context; if(!paced(b)) return SCAN_CANCELLED;
    int fd=source_socket(b,SOCK_STREAM,IPPROTO_TCP); if(fd<0) return SCAN_SYSTEM_ERROR;
    struct sockaddr_in target={0}; target.sin_family=AF_INET; target.sin_addr.s_addr=htonl(ip); target.sin_port=htons(port);
    int error=0;
    if(connect(fd,(struct sockaddr *)&target,sizeof(target))) {
        error=errno;
        if(error==EINPROGRESS) {
            int ready=wait_socket(b,fd,POLLOUT,milliseconds()+timeout);
            if(ready==-2) { close(fd); return SCAN_CANCELLED; }
            if(ready<=0) { close(fd); return ready==0 ? SCAN_TIMEOUT : SCAN_SYSTEM_ERROR; }
            socklen_t length=sizeof(error); if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)) error=errno;
        }
    }
    close(fd); if(!error) return SCAN_OK; if(error==ECONNREFUSED) return SCAN_REFUSED;
    return error==ENETUNREACH || error==EHOSTUNREACH ? SCAN_NO_ROUTE : SCAN_TIMEOUT;
}
static uint16_t checksum(const uint8_t *data,size_t n) {
    uint32_t sum=0; for(size_t i=0;i+1<n;i+=2) sum+=((uint32_t)data[i]<<8)|data[i+1];
    if(n&1) sum+=(uint32_t)data[n-1]<<8; while(sum>>16) sum=(sum&65535)+(sum>>16); return (uint16_t)~sum;
}
static scan_result_t ping(void *context,uint32_t ip,uint32_t timeout,lan_host_t *host) {
    posix_backend_t *b=context; if(!paced(b)) return SCAN_CANCELLED;
    int fd=source_socket(b,SOCK_DGRAM,IPPROTO_ICMP); if(fd<0) return SCAN_SYSTEM_ERROR;
    struct sockaddr_in target={0}; target.sin_family=AF_INET; target.sin_addr.s_addr=htonl(ip);
    uint8_t packet[16]={8,0,0,0,0,0,0,1,'L','O','O','K','L','A','N',0};
    uint16_t crc=checksum(packet,sizeof(packet)); packet[2]=(uint8_t)(crc>>8);packet[3]=(uint8_t)crc;
    uint64_t started=milliseconds(); if(sendto(fd,packet,sizeof(packet),0,(struct sockaddr *)&target,sizeof(target))<0) { close(fd);return SCAN_SYSTEM_ERROR; }
    uint64_t deadline=started+timeout; scan_result_t result=SCAN_TIMEOUT;
    for(;;) {
        int ready=wait_socket(b,fd,POLLIN,deadline); if(ready<=0) { if(ready==-2) result=SCAN_CANCELLED; break; }
        uint8_t reply[1024]; struct sockaddr_in peer={0}; socklen_t length=sizeof(peer);
        ssize_t bytes=recvfrom(fd,reply,sizeof(reply),0,(struct sockaddr *)&peer,&length);
        size_t offset=0; if(bytes>=20 && (reply[0]>>4)==4) offset=(reply[0]&15)*4u;
        if(bytes<(ssize_t)(offset+sizeof(packet)) || peer.sin_addr.s_addr!=target.sin_addr.s_addr || reply[offset]!=0 || reply[offset+1]!=0 || memcmp(reply+offset+8,packet+8,8)) continue;
        host->latency_ms=(uint32_t)(milliseconds()-started);host->has_latency=true;
        if(offset>=20) host->ttl=reply[8]; result=SCAN_OK;break;
    }
    close(fd);return result;
}
static scan_result_t neighbor(void *context,uint32_t ip,lan_host_t *host) {
    posix_backend_t *b=context;
    if(ip!=b->selected.ip) return SCAN_TIMEOUT; /* Never treat stale cache as liveness. */
    host->interface_index=b->selected.index;host->has_mac=b->selected.has_mac;memcpy(host->mac,b->selected.mac,6);gethostname(host->hostname,sizeof(host->hostname)-1);return SCAN_OK;
}
static void *worker(void *context) {
    posix_backend_t *b=context; platform_net_ops_t ops={neighbor,ping,cancelled,wall,tcp};
    while(!cancelled(b)) {
        pthread_mutex_lock(&b->lock); uint32_t offset=b->next++; pthread_mutex_unlock(&b->lock);
        if(offset>=b->total) break; uint32_t ip=0;
        if(!ranges_address_at(b->profile.ranges,b->profile.range_count,offset,&ip)) break;
        lan_host_t host; scan_result_t result=scanner_probe(&ops,b,ip,b->profile.icmp_timeout_ms,&host); host.interface_index=b->selected.index;
        if(result==SCAN_OK && !cancelled(b)) {
            cached_mac(&b->selected,ip,&host);
            emit(b,(posix_event_t){.type=PX_HOST,.host=host});
            scanner_services(&ops,b,&host,b->profile.ports,b->profile.port_count,b->profile.tcp_timeout_ms);
            if(!cancelled(b)) emit(b,(posix_event_t){.type=PX_HOST,.host=host});
        }
        pthread_mutex_lock(&b->lock); uint32_t done=++b->completed; pthread_mutex_unlock(&b->lock);
        emit(b,(posix_event_t){.type=PX_PROGRESS,.completed=done,.total=b->total});
    }
    return NULL;
}
static void *coordinator(void *context) {
    posix_backend_t *b=context; pthread_t threads[256]; size_t created=0;
    for(unsigned i=0;i<b->profile.concurrency && !cancelled(b);++i) {
        int error=pthread_create(&threads[created],NULL,worker,b);
        if(error) { emit(b,(posix_event_t){.type=PX_ERROR,.error=error}); atomic_store(&b->cancel,true);break; } ++created;
    }
    for(size_t i=0;i<created;++i) pthread_join(threads[i],NULL);
    emit(b,(posix_event_t){.type=PX_DONE,.completed=b->completed,.total=b->total});
    atomic_store(&b->running,false);return NULL;
}
bool posix_backend_init(posix_backend_t *b) { memset(b,0,sizeof(*b));atomic_init(&b->cancel,false);atomic_init(&b->running,false);return pthread_mutex_init(&b->lock,NULL)==0; }
bool posix_reap(posix_backend_t *b) { if(atomic_load(&b->running)) return false; if(b->joinable){pthread_join(b->coordinator,NULL);b->joinable=false;} return true; }
bool posix_scan_start(posix_backend_t *b,const posix_interface_t *selected,const profile_t *profile) {
    if(!posix_reap(b) || !profile_validate(profile,NULL,0)) return false;
    b->profile=*profile; uint64_t total=0; if(!ranges_normalize(b->profile.ranges,&b->profile.range_count,&total) || !total || total>65536) return false;
    b->selected=*selected;b->total=(uint32_t)total;b->next=b->completed=0;b->ticket=0;
    pthread_mutex_lock(&b->lock);b->count=b->head=0;pthread_mutex_unlock(&b->lock);
    atomic_store(&b->cancel,false);atomic_store(&b->running,true);
    int error=pthread_create(&b->coordinator,NULL,coordinator,b); if(error){atomic_store(&b->running,false);return false;} b->joinable=true;return true;
}
void posix_stop(posix_backend_t *b) { atomic_store(&b->cancel,true); }
bool posix_pop(posix_backend_t *b,posix_event_t *event) {
    pthread_mutex_lock(&b->lock);bool available=b->count!=0;
    if(available){*event=b->events[b->head];b->head=(b->head+1)%POSIX_EVENTS;--b->count;}pthread_mutex_unlock(&b->lock);return available;
}
bool posix_backend_destroy(posix_backend_t *b) { posix_stop(b);if(!posix_reap(b))return false;pthread_mutex_destroy(&b->lock);return true; }
