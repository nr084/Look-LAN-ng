#include "posix_neighbors.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/neighbour.h>
#else
#include <net/route.h>
#include <net/if_dl.h>
#include <sys/sysctl.h>
#endif
static void pause_ms(unsigned ms){struct timespec t={0,(long)ms*1000000};nanosleep(&t,NULL);}
static void observation(posix_backend_t *b,uint32_t ip,const uint8_t mac[6],bool live){size_t slot;if(!posix_target_slot(b,ip,&slot) || (mac[0]&1) || !(mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]))return;lan_host_t host={0};host.address=ipv4_address(ip);host.interface_index=b->selected.index;memcpy(host.mac,mac,6);host.has_mac=true;host.cache_only=!live;
    pthread_mutex_lock(&b->lock);memcpy(b->neighbor_mac[slot],mac,6);pthread_mutex_unlock(&b->lock);
    if(live){pthread_mutex_lock(&b->lock);b->neighbor_live[slot]=true;b->neighbor_proved[slot]=true;pthread_mutex_unlock(&b->lock);host.first_seen=host.last_seen=(uint64_t)time(NULL);host.online=true;posix_queue_name(b,ip);}posix_publish(b,(posix_event_t){.type=live ? PX_HOST:PX_CACHE,.host=host,.proof=live ? PX_PROOF_ARP:0});
}
#ifdef __linux__
static void linux_messages(posix_backend_t *b,const void *data,size_t bytes){int length=(int)bytes;for(struct nlmsghdr *h=(struct nlmsghdr *)data;length>=(int)sizeof(*h) && NLMSG_OK(h,(unsigned)length);h=NLMSG_NEXT(h,length)){
    if(h->nlmsg_type!=RTM_NEWNEIGH || h->nlmsg_len<NLMSG_LENGTH(sizeof(struct ndmsg)))continue;struct ndmsg *n=NLMSG_DATA(h);if(n->ndm_family!=AF_INET || (uint32_t)n->ndm_ifindex!=b->selected.index || !(n->ndm_state&(NUD_REACHABLE|NUD_STALE|NUD_DELAY|NUD_PROBE|NUD_PERMANENT)))continue;
    uint32_t ip=0;uint8_t mac[6];bool have_ip=false,have_mac=false;int payload=(int)h->nlmsg_len-(int)NLMSG_LENGTH(sizeof(*n));for(struct rtattr *a=(struct rtattr *)((char *)n+NLMSG_ALIGN(sizeof(*n)));RTA_OK(a,payload);a=RTA_NEXT(a,payload)){
        if(a->rta_type==NDA_DST && RTA_PAYLOAD(a)==4){uint32_t value;memcpy(&value,RTA_DATA(a),4);ip=ntohl(value);have_ip=true;}if(a->rta_type==NDA_LLADDR && RTA_PAYLOAD(a)==6){memcpy(mac,RTA_DATA(a),6);have_mac=true;}}
    if(have_ip && have_mac)observation(b,ip,mac,h->nlmsg_seq==0 && (n->ndm_state&NUD_REACHABLE));
}}
#else
static void mac_messages(posix_backend_t *b,const char *data,size_t bytes,bool live){for(size_t at=0;at+sizeof(struct rt_msghdr)<=bytes;){const struct rt_msghdr *r=(const struct rt_msghdr *)(data+at);size_t length=r->rtm_msglen;if(length<sizeof(*r) || length>bytes-at)break;
    if(r->rtm_version==RTM_VERSION && r->rtm_index==b->selected.index && (r->rtm_flags&RTF_LLINFO) && !r->rtm_errno && (!live || (!r->rtm_pid && (r->rtm_type==RTM_ADD || r->rtm_type==RTM_CHANGE)))){size_t offset=sizeof(*r);const struct sockaddr_in *destination=NULL;const struct sockaddr_dl *link=NULL;for(unsigned i=0;i<RTAX_MAX;++i)if(r->rtm_addrs&(1<<i)){if(offset+2>length)break;const struct sockaddr *s=(const struct sockaddr *)(data+at+offset);size_t aligned=s->sa_len ? (s->sa_len+3u)&~3u:4u;if(aligned>length-offset)break;if(i==RTAX_DST && s->sa_family==AF_INET && s->sa_len>=sizeof(struct sockaddr_in))destination=(const struct sockaddr_in *)s;if(i==RTAX_GATEWAY && s->sa_family==AF_LINK && s->sa_len>=offsetof(struct sockaddr_dl,sdl_data))link=(const struct sockaddr_dl *)s;offset+=aligned;}
        if(destination && link && link->sdl_alen==6 && offsetof(struct sockaddr_dl,sdl_data)+link->sdl_nlen+6<=link->sdl_len)observation(b,ntohl(destination->sin_addr.s_addr),(const uint8_t *)LLADDR(link),live);
    }at+=length;
}}
#endif
bool posix_neighbors_open(posix_backend_t *b){
#ifdef __linux__
    int fd=socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);if(fd<0)return false;struct sockaddr_nl address={0};address.nl_family=AF_NETLINK;address.nl_groups=RTMGRP_NEIGH;if(bind(fd,(struct sockaddr *)&address,sizeof(address))){close(fd);return false;}
    struct {struct nlmsghdr header;struct ndmsg message;} request={0};request.header.nlmsg_len=NLMSG_LENGTH(sizeof(request.message));request.header.nlmsg_type=RTM_GETNEIGH;request.header.nlmsg_flags=NLM_F_REQUEST|NLM_F_DUMP;request.header.nlmsg_seq=1;request.message.ndm_family=AF_INET;struct sockaddr_nl kernel={0};kernel.nl_family=AF_NETLINK;sendto(fd,&request,request.header.nlmsg_len,0,(struct sockaddr *)&kernel,sizeof(kernel));
#else
    int fd=socket(AF_ROUTE,SOCK_RAW,AF_INET);if(fd<0)return false;
#endif
    if(fcntl(fd,F_SETFL,O_NONBLOCK)<0 || fcntl(fd,F_SETFD,FD_CLOEXEC)<0){close(fd);return false;}b->neighbor_fd=fd;return true;
}
void *posix_neighbors_watch(void *context){posix_backend_t *b=context;
#ifdef __APPLE__
    int mib[]={CTL_NET,PF_ROUTE,0,AF_INET,NET_RT_FLAGS,RTF_LLINFO};size_t size=0;if(!sysctl(mib,6,NULL,&size,NULL,0) && size && size<=1024*1024){char *data=malloc(size);if(data){if(!sysctl(mib,6,data,&size,NULL,0))mac_messages(b,data,size,false);free(data);}}
#endif
    while(!atomic_load(&b->cancel) && !atomic_load(&b->neighbor_done)){struct pollfd p={b->neighbor_fd,POLLIN,0};if(poll(&p,1,25)<=0)continue;union {uint64_t alignment;char data[16384];} buffer;
#ifdef __linux__
        struct sockaddr_nl peer={0};socklen_t length=sizeof(peer);ssize_t bytes=recvfrom(b->neighbor_fd,buffer.data,sizeof(buffer.data),0,(struct sockaddr *)&peer,&length);if(bytes>0 && peer.nl_pid==0)linux_messages(b,buffer.data,(size_t)bytes);
#else
        ssize_t bytes=read(b->neighbor_fd,buffer.data,sizeof(buffer.data));if(bytes>0)mac_messages(b,buffer.data,(size_t)bytes,true);
#endif
    }return NULL;
}
scan_result_t posix_neighbor_probe(posix_backend_t *b,uint32_t ip,lan_host_t *host){size_t slot;if(b->neighbor_fd<0 || !posix_target_slot(b,ip,&slot))return SCAN_TIMEOUT;uint32_t mask=b->selected.prefix ? UINT32_MAX<<(32-b->selected.prefix):0;if((ip&mask)!=(b->selected.ip&mask) || (b->selected.prefix<31 && (ip==(ip&mask) || ip==((ip&mask)|~mask))))return SCAN_TIMEOUT;
    if(!posix_pace(b))return SCAN_CANCELLED;int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return SCAN_SYSTEM_ERROR;struct sockaddr_in source={0},target={0};source.sin_family=target.sin_family=AF_INET;source.sin_addr.s_addr=htonl(b->selected.ip);target.sin_addr.s_addr=htonl(ip);target.sin_port=htons(65535);bool valid=!bind(fd,(struct sockaddr *)&source,sizeof(source)) && fcntl(fd,F_SETFL,O_NONBLOCK)>=0 && fcntl(fd,F_SETFD,FD_CLOEXEC)>=0;
#ifdef __linux__
    uint32_t index=htonl(b->selected.index);valid=valid && !setsockopt(fd,IPPROTO_IP,IP_UNICAST_IF,&index,sizeof(index));
#else
    unsigned index=b->selected.index;valid=valid && !setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index));
#endif
    if(valid){pthread_mutex_lock(&b->lock);b->neighbor_requested[slot]=true;pthread_mutex_unlock(&b->lock);sendto(fd,"",0,0,(struct sockaddr *)&target,sizeof(target));}close(fd);if(!valid)return SCAN_SYSTEM_ERROR;
    for(unsigned elapsed=0;elapsed<75 && !atomic_load(&b->cancel);++elapsed){pthread_mutex_lock(&b->lock);bool live=b->neighbor_live[slot];if(live){memcpy(host->mac,b->neighbor_mac[slot],6);host->has_mac=true;}pthread_mutex_unlock(&b->lock);if(live)return SCAN_OK;pause_ms(1);}return atomic_load(&b->cancel) ? SCAN_CANCELLED:SCAN_TIMEOUT;
}
