#include "history.h"
#include <stdlib.h>
#include <string.h>
#define HEADER 176u
#define HOST 448u
#define EVENT 552u
static void put(unsigned char *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i){p[i]=(unsigned char)v;v>>=8;}}
static uint64_t get(const unsigned char *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)p[i]<<(i*8);return v;}
static uint32_t crc(uint32_t c,const unsigned char *p,size_t n){while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;}
static bool emit(text_writer_t w,void *ctx,uint32_t *sum,const unsigned char *p,size_t n){*sum=crc(*sum,p,n);return w(ctx,(const char *)p,n);}
bool history_write(const monitor_t *m,const profile_t *p,text_writer_t w,void *ctx){
    if(!m || !p || !w || !memchr(p->interface_id,0,128) || *m->count>HISTORY_MAX_HOSTS || m->log_count>HISTORY_MAX_EVENTS)return false;
    unsigned char b[EVENT]={0};uint32_t sum=~0u;
    memcpy(b,"LANHIST1",8);put(b+8,1,4);put(b+12,*m->count,4);put(b+16,m->log_count,4);put(b+20,m->baseline,4);put(b+24,m->round,8);put(b+32,m->sequence,8);memcpy(b+40,p->interface_id,strlen(p->interface_id));
    if(!emit(w,ctx,&sum,b,HEADER))return false;
    for(size_t i=0;i<*m->count;++i){const lan_host_t *h=&m->hosts[i];memset(b,0,HOST);
        put(b,ipv4_value(&h->address),4);memcpy(b+4,h->hostname,256);memcpy(b+260,h->mac,6);
        unsigned flags=(h->has_mac?1u:0u)|(h->online?2u:0u)|(h->cache_only?4u:0u)|(h->new_device?8u:0u)|(h->changed?16u:0u)|(h->ports_checked?32u:0u)|(h->ports_truncated?64u:0u);
        put(b+266,flags,2);b[268]=(unsigned char)h->os_family;b[269]=h->os_confidence;b[270]=h->ttl;b[271]=h->port_count;
        put(b+272,h->first_seen,8);put(b+280,h->last_seen,8);for(unsigned k=0;k<h->port_count && k<64;++k)put(b+288+k*2,h->open_ports[k],2);
        if(h->address.family!=4 || h->port_count>64 || !memchr(h->hostname,0,256) || !emit(w,ctx,&sum,b,HOST))return false;
    }
    for(size_t i=0;i<m->log_count;++i){const monitor_event_t *e=monitor_log_at(m,i);memset(b,0,EVENT);put(b,e->sequence,8);put(b+8,e->timestamp,8);put(b+16,e->type,4);put(b+20,e->host_index==SIZE_MAX?UINT32_MAX:e->host_index,4);put(b+24,e->address.family==4?ipv4_value(&e->address):0,4);memcpy(b+28,e->previous,256);memcpy(b+284,e->current,256);
        if(!emit(w,ctx,&sum,b,EVENT))return false;
    }
    put(b,sum^~0u,4);return w(ctx,(const char *)b,4);
}
bool history_read(const void *data,size_t n,monitor_t *m,const profile_t *p,uint32_t nic){
    const unsigned char *s=data;if(!s || !m || !p || n<HEADER+4 || memcmp(s,"LANHIST1",8) || get(s+8,4)!=1 || !memchr(s+40,0,128) || strncmp((const char *)s+40,p->interface_id,128))return false;
    size_t hosts=(size_t)get(s+12,4),events=(size_t)get(s+16,4);
    if(hosts>HISTORY_MAX_HOSTS || hosts>m->capacity || events>HISTORY_MAX_EVENTS || events>m->log_capacity || get(s+20,4)>1 || get(s+24,8)==UINT64_MAX || get(s+32,8)==UINT64_MAX || n!=HEADER+hosts*HOST+events*EVENT+4 || (crc(~0u,s,n-4)^~0u)!=get(s+n-4,4))return false;
    lan_host_t *h=calloc(hosts?hosts:1,sizeof(*h));monitor_event_t *log=calloc(events?events:1,sizeof(*log));bool ok=h && log;uint64_t last=0;
    for(size_t i=0;ok && i<hosts;++i){const unsigned char *b=s+HEADER+i*HOST;unsigned flags=(unsigned)get(b+266,2);
        if(!memchr(b+4,0,256) || flags>127 || b[268]>OS_EMBEDDED || b[269]>100 || b[271]>64 || get(b+272,8)>get(b+280,8)){ok=false;break;}
        h[i].address=ipv4_address((uint32_t)get(b,4));memcpy(h[i].hostname,b+4,256);memcpy(h[i].mac,b+260,6);h[i].has_mac=(flags&1)!=0;h[i].online=(flags&2)!=0;h[i].cache_only=(flags&4)!=0;h[i].new_device=(flags&8)!=0;h[i].changed=(flags&16)!=0;h[i].ports_checked=(flags&32)!=0;h[i].ports_truncated=(flags&64)!=0;
        h[i].os_family=(os_family_t)b[268];h[i].os_confidence=b[269];h[i].ttl=b[270];h[i].port_count=b[271];h[i].first_seen=get(b+272,8);h[i].last_seen=get(b+280,8);h[i].interface_index=nic;h[i].historical=true;
        for(unsigned k=0;k<h[i].port_count;++k)h[i].open_ports[k]=(uint16_t)get(b+288+k*2,2);
    }
    for(size_t i=0;ok && i<events;++i){const unsigned char *b=s+HEADER+hosts*HOST+i*EVENT;uint64_t index=get(b+20,4);monitor_event_t *e=&log[i];
        if(!memchr(b+28,0,256) || !memchr(b+284,0,256) || get(b+16,4)>MON_EXPORT_ERROR || (index!=UINT32_MAX && index>=hosts) || get(b,8)<=last || get(b,8)>get(s+32,8)){ok=false;break;}
        e->sequence=last=get(b,8);e->timestamp=get(b+8,8);e->type=(monitor_event_type_t)get(b+16,4);e->host_index=index==UINT32_MAX?SIZE_MAX:(size_t)index;e->address=get(b+24,4)?ipv4_address((uint32_t)get(b+24,4)):(ip_address_t){0};memcpy(e->previous,b+28,256);memcpy(e->current,b+284,256);
    }
    if(ok){memcpy(m->hosts,h,hosts*sizeof(*h));memcpy(m->log,log,events*sizeof(*log));*m->count=hosts;m->log_head=0;m->log_count=events;m->sequence=get(s+32,8);m->round=get(s+24,8);m->baseline=get(s+20,4)!=0;m->interface_index=nic;}
    free(h);free(log);return ok;
}
