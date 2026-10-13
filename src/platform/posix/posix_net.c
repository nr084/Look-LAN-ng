#include "text.h"
#include "posix_net.h"
#include "posix_names.h"
#include "posix_tools.h"
#include "posix_neighbors.h"
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
#include <SystemConfiguration/SystemConfiguration.h>
#endif
static void interface_info(posix_interface_t *nic){
#ifdef __APPLE__
    SCDynamicStoreRef store=SCDynamicStoreCreate(NULL,CFSTR("LookLAN-ng"),NULL,NULL);SCPreferencesRef prefs=SCPreferencesCreate(NULL,CFSTR("LookLAN-ng"),NULL);CFArrayRef services=prefs ? SCNetworkServiceCopyAll(prefs):NULL;CFStringRef name=CFStringCreateWithCString(NULL,nic->name,kCFStringEncodingUTF8);
    if(store && services && name)for(CFIndex i=0;i<CFArrayGetCount(services);++i){SCNetworkServiceRef service=(SCNetworkServiceRef)CFArrayGetValueAtIndex(services,i);SCNetworkInterfaceRef iface=SCNetworkServiceGetInterface(service);CFStringRef bsd=iface ? SCNetworkInterfaceGetBSDName(iface):NULL;if(!bsd || !CFEqual(name,bsd))continue;CFStringRef id=SCNetworkServiceGetServiceID(service);
        CFStringRef key=SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,kSCDynamicStoreDomainState,id,kSCEntNetIPv4);CFPropertyListRef value=key ? SCDynamicStoreCopyValue(store,key):NULL;if(key)CFRelease(key);char text[64];
        if(value && CFGetTypeID(value)==CFDictionaryGetTypeID()){CFStringRef router=CFDictionaryGetValue((CFDictionaryRef)value,kSCPropNetIPv4Router);if(router && CFGetTypeID(router)==CFStringGetTypeID() && CFStringGetCString(router,text,sizeof(text),kCFStringEncodingUTF8))ipv4_parse(text,strlen(text),&nic->gateway);}if(value)CFRelease(value);
        key=SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,kSCDynamicStoreDomainState,id,kSCEntNetDNS);value=key ? SCDynamicStoreCopyValue(store,key):NULL;if(key)CFRelease(key);
        if(value && CFGetTypeID(value)==CFDictionaryGetTypeID()){CFArrayRef servers=CFDictionaryGetValue((CFDictionaryRef)value,kSCPropNetDNSServerAddresses);if(servers && CFGetTypeID(servers)==CFArrayGetTypeID())for(CFIndex j=0;j<CFArrayGetCount(servers) && nic->dns_count<3;++j){CFStringRef s=CFArrayGetValueAtIndex(servers,j);if(s && CFGetTypeID(s)==CFStringGetTypeID() && CFStringGetCString(s,text,sizeof(text),kCFStringEncodingUTF8) && ipv4_parse(text,strlen(text),&nic->dns[nic->dns_count]))++nic->dns_count;}}if(value)CFRelease(value);break;
    }
    if(name)CFRelease(name);if(services)CFRelease(services);if(prefs)CFRelease(prefs);if(store)CFRelease(store);
#else
    FILE *f=fopen("/proc/net/route","r");char line[512];if(f){while(fgets(line,sizeof(line),f)){char name[128];unsigned long destination,gateway;unsigned flags,metric;if(sscanf(line,"%127s %lx %lx %x %*u %*u %u",name,&destination,&gateway,&flags,&metric)==5 && !strcmp(name,nic->name) && !destination && (flags&3)==3 && (!nic->has_metric || metric<nic->metric)){nic->gateway=ntohl((uint32_t)gateway);nic->metric=metric;nic->has_metric=true;}}fclose(f);}
    char path[256];snprintf(path,sizeof(path),"/run/systemd/netif/links/%u",nic->index);f=fopen(path,"r");if(f){while(fgets(line,sizeof(line),f))if(!strncmp(line,"DNS=",4)){char *save=NULL;for(char *s=strtok_r(line+4," \r\n",&save);s && nic->dns_count<3;s=strtok_r(NULL," \r\n",&save))if(ipv4_parse(s,strlen(s),&nic->dns[nic->dns_count]))++nic->dns_count;}fclose(f);}
#endif
    if(!nic->dns_count){FILE *f=fopen("/etc/resolv.conf","r");char line[256];if(f){while(nic->dns_count<3 && fgets(line,sizeof(line),f)){char s[64];if(sscanf(line,"nameserver %63s",s)==1 && ipv4_parse(s,strlen(s),&nic->dns[nic->dns_count]))++nic->dns_count;}fclose(f);}}
}
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
                size_t bytes=sa->sa_len ? (sa->sa_len+3u)&~3u : 4u;
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
        nic->automatic=(it->ifa_flags&IFF_RUNNING) && !(it->ifa_flags&IFF_POINTOPOINT) && (nic->ip>>16)!=0xa9fe;const char *manual[]={"tun","tap","utun","vmnet","vbox","virbr","docker","veth","br-","wg","bridge","vlan","feth","gif","stf"};for(size_t k=0;k<sizeof(manual)/sizeof(*manual);++k)if(!strncmp(nic->name,manual[k],strlen(manual[k])))nic->automatic=false;
#ifdef __linux__
        char hardware[256];snprintf(hardware,sizeof(hardware),"/sys/class/net/%s/device",nic->name);if(access(hardware,F_OK))nic->automatic=false;
#endif
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
    for(size_t i=0;i<count;++i)interface_info(&out[i]);
    freeifaddrs(list); return count;
}
bool posix_cancelled(const posix_backend_t *b){return atomic_load(&b->cancel) || (b->owner && (atomic_load(&b->owner->cancel) || atomic_load(&b->owner->shutdown) || atomic_load(&b->owner->generation)!=b->provider_generation));}
static bool cancelled(void *context) { return posix_cancelled(context); }
static uint64_t wall(void *context) { (void)context; return (uint64_t)time(NULL); }
static bool interface_alive(const posix_interface_t *nic){struct ifaddrs *list=NULL;if(getifaddrs(&list))return false;bool found=false;for(struct ifaddrs *i=list;i;i=i->ifa_next)if(i->ifa_addr && i->ifa_addr->sa_family==AF_INET && (i->ifa_flags&IFF_UP) && (i->ifa_flags&IFF_RUNNING) && !strcmp(i->ifa_name,nic->name) && ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr)==nic->ip){found=true;break;}freeifaddrs(list);return found;}
static bool emit(posix_backend_t *b,posix_event_t event) {
    if(!event.generation)event.generation=atomic_load(&b->generation);
    for(;;) {
        if(event.generation!=atomic_load(&b->generation))return false;
        pthread_mutex_lock(&b->lock);
        if(b->count<POSIX_EVENTS) { b->events[(b->head+b->count)%POSIX_EVENTS]=event; ++b->count; pthread_mutex_unlock(&b->lock); return true; }
        pthread_mutex_unlock(&b->lock); if(cancelled(b)) return false; pause_ms(10);
    }
}
bool posix_publish(posix_backend_t *b,posix_event_t event){return emit(b,event);}
bool posix_target_slot(const posix_backend_t *b,uint32_t ip,size_t *slot){size_t offset=0;for(size_t i=0;i<b->profile.range_count;++i){const ipv4_range_t *r=&b->profile.ranges[i];if(ip>=r->first && ip<=r->last){*slot=offset+ip-r->first;return *slot<65536;}offset+=(size_t)((uint64_t)r->last-r->first+1);}return false;}
void posix_queue_name(posix_backend_t *b,uint32_t ip){size_t slot;if(!b->provider_count || !posix_target_slot(b,ip,&slot))return;char cached[256]="";uint8_t cached_mac[6]={0};uint64_t generation=atomic_load(&b->generation),now=milliseconds();pthread_mutex_lock(&b->lock);if(b->name_queued[slot] && memcmp(b->name_mac[slot],b->neighbor_mac[slot],6))b->name_queued[slot]=false;if(!b->name_queued[slot]){memcpy(b->name_mac[slot],b->neighbor_mac[slot],6);bool has_mac=b->neighbor_mac[slot][0] || b->neighbor_mac[slot][1] || b->neighbor_mac[slot][2] || b->neighbor_mac[slot][3] || b->neighbor_mac[slot][4] || b->neighbor_mac[slot][5];bool found=false;for(unsigned i=0;i<1024;++i){posix_name_cache_t *c=&b->name_cache[i];if(c->ip==ip && c->interface_index==b->selected.index && c->source==b->selected.ip && c->expires>now && (!has_mac || (c->has_mac && !memcmp(c->mac,b->neighbor_mac[slot],6)))){lan_text_copy(cached,sizeof(cached),c->name);memcpy(cached_mac,b->neighbor_mac[slot],6);found=true;break;}}if(found)b->name_queued[slot]=true;else if(b->name_count<4096){posix_name_request_t *job=&b->names[(b->name_head+b->name_count)%4096];job->ip=ip;job->nic=b->selected;job->generation=generation;job->has_mac=has_mac;memcpy(job->mac,b->neighbor_mac[slot],6);++b->name_count;b->name_queued[slot]=true;}}pthread_mutex_unlock(&b->lock);if(cached[0]){lan_host_t host={0};host.address=ipv4_address(ip);host.interface_index=b->selected.index;lan_text_copy(host.hostname,sizeof(host.hostname),cached);memcpy(host.mac,cached_mac,6);host.has_mac=!!(host.mac[0]|host.mac[1]|host.mac[2]|host.mac[3]|host.mac[4]|host.mac[5]);emit(b,(posix_event_t){.type=PX_NAME,.host=host,.generation=generation});}}
bool posix_names_pending(posix_backend_t *b){pthread_mutex_lock(&b->lock);bool pending=b->name_count!=0;pthread_mutex_unlock(&b->lock);return pending || atomic_load(&b->providers_busy)!=0;}
void posix_invalidate_names(posix_backend_t *b){atomic_fetch_add(&b->generation,1);pthread_mutex_lock(&b->lock);b->name_head=b->name_count=0;pthread_mutex_unlock(&b->lock);}
static int wait_socket(posix_backend_t *b,int fd,short flags,uint64_t deadline) {
    while(!cancelled(b)) {
        uint64_t now=milliseconds(); if(now>=deadline) return 0;
        int delay=(int)(deadline-now); if(delay>25) delay=25;
        struct pollfd p={fd,flags,0}; int result=poll(&p,1,delay);
        if(result>0) return 1; if(result<0 && errno!=EINTR) return -1;
    }
    return -2;
}
static int source_socket(posix_backend_t *b,int type,int protocol,uint32_t target) {
    int fd=socket(AF_INET,type,protocol); if(fd<0) return -1;
    struct sockaddr_in source={0}; source.sin_family=AF_INET; source.sin_addr.s_addr=htonl(b->selected.ip);
    if(bind(fd,(struct sockaddr *)&source,sizeof(source)) || fcntl(fd,F_SETFL,O_NONBLOCK)<0 || fcntl(fd,F_SETFD,FD_CLOEXEC)<0) { close(fd); return -1; }
#ifdef __APPLE__
    unsigned index=b->selected.index;
    if(target!=b->selected.ip && (target>>24)!=127 && setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index))) { close(fd); return -1; }
    int no_signal=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&no_signal,sizeof(no_signal));
#elif defined(IP_UNICAST_IF)
    if((target>>24)!=127){uint32_t index=htonl(b->selected.index);if(setsockopt(fd,IPPROTO_IP,IP_UNICAST_IF,&index,sizeof(index))){close(fd);return -1;}}
#endif
    return fd;
}
static bool paced(posix_backend_t *b) {
    if(b->owner){if(cancelled(b))return false;return paced(b->owner) && !cancelled(b);}
    pthread_mutex_lock(&b->lock); uint64_t now=milliseconds(); if(b->ticket<now) b->ticket=now; uint64_t due=b->ticket;unsigned pace=atomic_load(&b->pace_ms);b->ticket+=pace ? pace:4; pthread_mutex_unlock(&b->lock);
    while(!cancelled(b) && milliseconds()<due) pause_ms(1);
    return !cancelled(b);
}
bool posix_pace(posix_backend_t *b){return paced(b);}
static scan_result_t tcp(void *context,uint32_t ip,uint16_t port,uint32_t timeout) {
    posix_backend_t *b=context; if(!paced(b)) return SCAN_CANCELLED;
    int fd=source_socket(b,SOCK_STREAM,IPPROTO_TCP,ip); if(fd<0) return SCAN_SYSTEM_ERROR;
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
    return error==ENETUNREACH || error==EHOSTUNREACH || error==EHOSTDOWN ? SCAN_NO_ROUTE : SCAN_TIMEOUT;
}
static uint16_t checksum(const uint8_t *data,size_t n) {
    uint32_t sum=0; for(size_t i=0;i+1<n;i+=2) sum+=((uint32_t)data[i]<<8)|data[i+1];
    if(n&1) sum+=(uint32_t)data[n-1]<<8; while(sum>>16) sum=(sum&65535)+(sum>>16); return (uint16_t)~sum;
}
static scan_result_t ping(void *context,uint32_t ip,uint32_t timeout,lan_host_t *host) {
    posix_backend_t *b=context; if(!paced(b)) return SCAN_CANCELLED;
    int fd=source_socket(b,SOCK_DGRAM,IPPROTO_ICMP,ip); if(fd<0) return SCAN_SYSTEM_ERROR;
#ifdef IP_RECVTTL
    int ttl_info=1;setsockopt(fd,IPPROTO_IP,IP_RECVTTL,&ttl_info,sizeof(ttl_info));
#endif
    struct sockaddr_in target={0}; target.sin_family=AF_INET; target.sin_addr.s_addr=htonl(ip);
    uint8_t packet[16]={8,0,0,0,0,0,0,1,'L','O','O','K','L','A','N',0};
    uint16_t crc=checksum(packet,sizeof(packet)); packet[2]=(uint8_t)(crc>>8);packet[3]=(uint8_t)crc;
    uint64_t started=milliseconds(); if(sendto(fd,packet,sizeof(packet),0,(struct sockaddr *)&target,sizeof(target))<0) {
        int error=errno;close(fd);
        /* A failed neighbor/route for one target is a normal probe outcome,
           not a backend failure that invalidates the entire monitoring round. */
        if(error==ENETUNREACH || error==EHOSTUNREACH || error==EHOSTDOWN)return SCAN_NO_ROUTE;
        if(error==EAGAIN || error==ENOBUFS)return SCAN_TIMEOUT;
        return SCAN_SYSTEM_ERROR;
    }
    uint64_t deadline=started+timeout; scan_result_t result=SCAN_TIMEOUT;
    for(;;) {
        int ready=wait_socket(b,fd,POLLIN,deadline); if(ready<=0) { if(ready==-2) result=SCAN_CANCELLED; break; }
        uint8_t reply[1024];struct sockaddr_in peer={0};union {struct cmsghdr alignment;char bytes[256];} control;struct iovec iov={reply,sizeof(reply)};struct msghdr message={0};message.msg_name=&peer;message.msg_namelen=sizeof(peer);message.msg_iov=&iov;message.msg_iovlen=1;message.msg_control=control.bytes;message.msg_controllen=sizeof(control.bytes);
        ssize_t bytes=recvmsg(fd,&message,0);
        size_t offset=0; if(bytes>=20 && (reply[0]>>4)==4) offset=(reply[0]&15)*4u;
        if(bytes<(ssize_t)(offset+sizeof(packet)) || peer.sin_addr.s_addr!=target.sin_addr.s_addr || reply[offset]!=0 || reply[offset+1]!=0 || reply[offset+6]!=0 || reply[offset+7]!=1 || memcmp(reply+offset+8,packet+8,8)) continue;
        host->latency_ms=(uint32_t)(milliseconds()-started);host->has_latency=true;
        if(offset>=20)host->ttl=reply[8];for(struct cmsghdr *c=CMSG_FIRSTHDR(&message);c;c=CMSG_NXTHDR(&message,c))if(c->cmsg_level==IPPROTO_IP && c->cmsg_type==IP_TTL && c->cmsg_len>=CMSG_LEN(sizeof(int))){int ttl;memcpy(&ttl,CMSG_DATA(c),sizeof(ttl));if(ttl>0 && ttl<256)host->ttl=(uint8_t)ttl;}result=SCAN_OK;break;
    }
    close(fd);return result;
}
scan_result_t posix_echo(posix_backend_t *backend,uint32_t target,unsigned timeout,lan_host_t *host){return ping(backend,target,timeout,host);}
static scan_result_t neighbor(void *context,uint32_t ip,lan_host_t *host) {
    posix_backend_t *b=context;
    if(ip!=b->selected.ip)return posix_neighbor_probe(b,ip,host);
    host->interface_index=b->selected.index;host->has_mac=b->selected.has_mac;memcpy(host->mac,b->selected.mac,6);gethostname(host->hostname,sizeof(host->hostname)-1);return SCAN_OK;
}
static void *name_worker(void *context){
    posix_backend_t *b=context,*local=calloc(1,sizeof(*local));if(!local || !posix_backend_init(local)){free(local);atomic_fetch_sub(&b->providers_alive,1);return NULL;}local->owner=b;
    while(!atomic_load(&b->shutdown)){pthread_mutex_lock(&b->lock);posix_name_request_t job={0};bool available=b->name_count!=0;if(available){job=b->names[b->name_head];b->name_head=(b->name_head+1)%4096;--b->name_count;atomic_fetch_add(&b->providers_busy,1);}pthread_mutex_unlock(&b->lock);if(!available){pause_ms(10);continue;}
        if(job.generation!=atomic_load(&b->generation) || cancelled(b)){atomic_fetch_sub(&b->providers_busy,1);continue;}local->selected=job.nic;local->provider_generation=job.generation;lan_host_t h={0};h.address=ipv4_address(job.ip);h.interface_index=job.nic.index;h.has_mac=job.has_mac;memcpy(h.mac,job.mac,6);posix_name(local,job.ip,h.hostname);if(!cancelled(local)){pthread_mutex_lock(&b->lock);if(job.generation==atomic_load(&b->generation)){posix_name_cache_t *c=&b->name_cache[b->cache_next++%1024];c->ip=job.ip;c->interface_index=job.nic.index;c->source=job.nic.ip;c->expires=milliseconds()+(h.hostname[0] ? 60000:10000);c->has_mac=job.has_mac;memcpy(c->mac,job.mac,6);lan_text_copy(c->name,sizeof(c->name),h.hostname);}pthread_mutex_unlock(&b->lock);}if(h.hostname[0] && !cancelled(local))emit(b,(posix_event_t){.type=PX_NAME,.host=h,.generation=job.generation});atomic_fetch_sub(&b->providers_busy,1);
    }pthread_mutex_destroy(&local->lock);free(local);atomic_fetch_sub(&b->providers_alive,1);return NULL;
}
typedef struct {posix_backend_t *backend;unsigned proof;} probe_context_t;
static scan_result_t probe_neighbor(void *context,uint32_t ip,lan_host_t *h){probe_context_t *c=context;scan_result_t result=neighbor(c->backend,ip,h);if(result==SCAN_OK)c->proof|=ip==c->backend->selected.ip ? PX_PROOF_LOCAL:PX_PROOF_ARP;return result;}
static scan_result_t probe_ping(void *context,uint32_t ip,uint32_t timeout,lan_host_t *h){probe_context_t *c=context;scan_result_t result=ping(c->backend,ip,timeout,h);if(result==SCAN_OK)c->proof|=PX_PROOF_IP;if(result!=SCAN_CANCELLED)emit(c->backend,(posix_event_t){.type=PX_PING,.host=*h,.error=(int)result});return result;}
static scan_result_t probe_tcp(void *context,uint32_t ip,uint16_t port,uint32_t timeout){probe_context_t *c=context;scan_result_t result=tcp(c->backend,ip,port,timeout);if(result==SCAN_OK || result==SCAN_REFUSED)c->proof|=PX_PROOF_IP;return result;}
static bool probe_cancelled(void *context){return cancelled(((probe_context_t *)context)->backend);}
static void *worker(void *context) {
    posix_backend_t *b=context; platform_net_ops_t ops={probe_neighbor,probe_ping,probe_cancelled,wall,probe_tcp};
    while(!cancelled(b)) {
        pthread_mutex_lock(&b->lock); uint32_t offset=b->next++; pthread_mutex_unlock(&b->lock);
        if(offset>=b->total) break; uint32_t ip=0;
        if(!ranges_address_at(b->profile.ranges,b->profile.range_count,offset,&ip)) break;
        probe_context_t probe={b,0};lan_host_t host; scan_result_t result=scanner_probe(&ops,&probe,ip,b->profile.icmp_timeout_ms,&host); host.interface_index=b->selected.index;
        if(result==SCAN_SYSTEM_ERROR && !atomic_exchange(&b->system_error,true))emit(b,(posix_event_t){.type=PX_ERROR,.error=EIO});
        if(result==SCAN_OK && !cancelled(b)) {
            cached_mac(&b->selected,ip,&host);
            if(host.has_mac){pthread_mutex_lock(&b->lock);memcpy(b->neighbor_mac[offset],host.mac,6);pthread_mutex_unlock(&b->lock);}
            pthread_mutex_lock(&b->lock);b->neighbor_proved[offset]=true;pthread_mutex_unlock(&b->lock);emit(b,(posix_event_t){.type=PX_HOST,.host=host,.proof=probe.proof});
            if(!host.hostname[0])posix_queue_name(b,ip);
            if(!b->monitoring)scanner_services(&ops,&probe,&host,b->profile.ports,b->profile.port_count,b->profile.tcp_timeout_ms);
            if(!cancelled(b)) emit(b,(posix_event_t){.type=PX_HOST,.host=host,.proof=probe.proof});
        }
        pthread_mutex_lock(&b->lock); uint32_t done=++b->completed; pthread_mutex_unlock(&b->lock);
        emit(b,(posix_event_t){.type=PX_PROGRESS,.completed=done,.total=b->total});
    }
    return NULL;
}
static void *coordinator(void *context) {
    posix_backend_t *b=context; pthread_t threads[256],neighbors;size_t created=0;bool have_neighbors=false;
    if(!interface_alive(&b->selected)){emit(b,(posix_event_t){.type=PX_ERROR,.error=ENETDOWN});atomic_store(&b->running,false);return NULL;}
    if(posix_neighbors_open(b)){if(!pthread_create(&neighbors,NULL,posix_neighbors_watch,b))have_neighbors=true;else{close(b->neighbor_fd);b->neighbor_fd=-1;}}
    for(unsigned i=0;i<b->profile.concurrency && i<b->total && !cancelled(b);++i) {
        int error=pthread_create(&threads[created],NULL,worker,b);
        if(error) { emit(b,(posix_event_t){.type=PX_ERROR,.error=error}); atomic_store(&b->cancel,true);break; } ++created;
    }
    for(size_t i=0;i<created;++i) pthread_join(threads[i],NULL);
    if(have_neighbors){bool pending=false;pthread_mutex_lock(&b->lock);for(uint32_t i=0;i<b->total;++i)if(b->neighbor_requested[i] && !b->neighbor_proved[i]){pending=true;break;}pthread_mutex_unlock(&b->lock);uint64_t deadline=milliseconds()+3000;while(pending && !cancelled(b) && milliseconds()<deadline)pause_ms(10);atomic_store(&b->neighbor_done,true);pthread_join(neighbors,NULL);close(b->neighbor_fd);b->neighbor_fd=-1;}
    if(!cancelled(b) && !interface_alive(&b->selected))emit(b,(posix_event_t){.type=PX_ERROR,.error=ENETDOWN});
    emit(b,(posix_event_t){.type=PX_DONE,.completed=b->completed,.total=b->total});
    atomic_store(&b->running,false);return NULL;
}
bool posix_backend_init(posix_backend_t *b) { memset(b,0,sizeof(*b));b->neighbor_fd=-1;atomic_init(&b->cancel,false);atomic_init(&b->running,false);atomic_init(&b->system_error,false);atomic_init(&b->shutdown,false);atomic_init(&b->neighbor_done,false);atomic_init(&b->generation,1);atomic_init(&b->pace_ms,4);atomic_init(&b->providers_alive,0);atomic_init(&b->providers_busy,0);return pthread_mutex_init(&b->lock,NULL)==0; }
bool posix_reap(posix_backend_t *b) { if(atomic_load(&b->running)) return false; if(b->joinable){pthread_join(b->coordinator,NULL);b->joinable=false;} return true; }
bool posix_scan_start(posix_backend_t *b,const posix_interface_t *selected,const profile_t *profile) {
    if(!posix_reap(b) || !profile_validate(profile,NULL,0)) return false;
    posix_invalidate_names(b);
    if(!b->providers_started){b->providers_started=true;for(unsigned i=0;i<2;++i){atomic_fetch_add(&b->providers_alive,1);if(!pthread_create(&b->providers[b->provider_count],NULL,name_worker,b))++b->provider_count;else atomic_fetch_sub(&b->providers_alive,1);}}
    b->profile=*profile; uint64_t total=0; if(!ranges_normalize(b->profile.ranges,&b->profile.range_count,&total) || !total || total>65536) return false;
    b->selected=*selected;b->total=(uint32_t)total;b->next=b->completed=0;atomic_store(&b->neighbor_done,false);memset(b->neighbor_mac,0,sizeof(b->neighbor_mac));memset(b->name_mac,0,sizeof(b->name_mac));memset(b->neighbor_live,0,sizeof(b->neighbor_live));memset(b->neighbor_requested,0,sizeof(b->neighbor_requested));memset(b->neighbor_proved,0,sizeof(b->neighbor_proved));memset(b->name_queued,0,sizeof(b->name_queued));
    pthread_mutex_lock(&b->lock);b->count=b->head=0;pthread_mutex_unlock(&b->lock);
    atomic_store(&b->cancel,false);atomic_store(&b->system_error,false);atomic_store(&b->running,true);
    int error=pthread_create(&b->coordinator,NULL,coordinator,b); if(error){atomic_store(&b->running,false);return false;} b->joinable=true;return true;
}
void posix_stop(posix_backend_t *b) { atomic_store(&b->cancel,true);posix_invalidate_names(b); }
bool posix_pop(posix_backend_t *b,posix_event_t *event) {
    pthread_mutex_lock(&b->lock);bool available=b->count!=0;
    if(available){*event=b->events[b->head];b->head=(b->head+1)%POSIX_EVENTS;--b->count;}pthread_mutex_unlock(&b->lock);return available;
}
bool posix_backend_destroy(posix_backend_t *b) {atomic_store(&b->shutdown,true);posix_stop(b);if(!posix_reap(b) || atomic_load(&b->providers_alive))return false;for(size_t i=0;i<b->provider_count;++i)pthread_join(b->providers[i],NULL);b->provider_count=0;pthread_mutex_destroy(&b->lock);return true;}
