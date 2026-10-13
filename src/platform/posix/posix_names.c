#include "text.h"
#include "posix_names.h"
#include "nbns.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
static uint16_t word(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void query_name(uint32_t ip,char out[64]){snprintf(out,64,"%u.%u.%u.%u.in-addr.arpa",ip&255,(ip>>8)&255,(ip>>16)&255,ip>>24);}
size_t posix_ptr_request(uint32_t ip,uint16_t tx,uint8_t p[512]){
    memset(p,0,512);p[0]=tx>>8;p[1]=tx;p[2]=1;p[5]=1;char name[64];query_name(ip,name);size_t used=12;const char *start=name;
    for(const char *s=name;;++s)if(*s=='.' || !*s){size_t n=s-start;p[used++]=(uint8_t)n;memcpy(p+used,start,n);used+=n;if(!*s)break;start=s+1;}
    p[used++]=0;p[used++]=0;p[used++]=12;p[used++]=0;p[used++]=1;return used;
}
static bool decode(const uint8_t *p,size_t n,size_t *at,char out[256]){
    size_t offset=*at,used=0,end=0;bool jumped=false;
    for(unsigned steps=0;steps<128 && offset<n;++steps){unsigned bytes=p[offset++];if(!bytes){if(!jumped)end=offset;*at=end;out[used]=0;return true;}
        if((bytes&0xc0)==0xc0){if(offset>=n)return false;size_t next=((bytes&63)<<8)|p[offset++];if(next>=n)return false;if(!jumped)end=offset;jumped=true;offset=next;continue;}
        if(bytes>63 || bytes>n-offset || used+bytes+(used ? 1:0)>=255)return false;if(used)out[used++]='.';
        for(unsigned i=0;i<bytes;++i){unsigned c=p[offset++];if(c<33 || c>126 || c=='.')return false;out[used++]=(char)c;}
    }return false;
}
bool posix_ptr_reply(const uint8_t *p,size_t n,uint32_t ip,uint16_t tx,char name[256]){
    if(n<12 || word(p)!=tx || (word(p+2)&0x800f)!=0x8000 || (word(p+2)&0x7a00) || word(p+4)!=1 || word(p+6)>64)return false;
    size_t at=12;char value[256],current[256];query_name(ip,current);
    if(!decode(p,n,&at,value) || strcasecmp(value,current) || n-at<4 || word(p+at)!=12 || word(p+at+2)!=1)return false;at+=4;
    struct record {char owner[256];size_t data;unsigned type,length;} records[64];unsigned count=0;
    for(unsigned i=0;i<word(p+6);++i){
        if(!decode(p,n,&at,value) || n-at<10)return false;
        unsigned type=word(p+at),cls=word(p+at+2),bytes=word(p+at+8);at+=10;if(bytes>n-at)return false;
        if(cls==1){lan_text_copy(records[count].owner,sizeof(records[count].owner),value);records[count].data=at;records[count].type=type;records[count++].length=bytes;}at+=bytes;
    }
    for(unsigned step=0;step<64;++step){
        for(unsigned i=0;i<count;++i)if(records[i].type==12 && !strcasecmp(records[i].owner,current)){
            size_t end=records[i].data;if(!decode(p,n,&end,value) || end!=records[i].data+records[i].length || !value[0])return false;
            return lan_text_copy(name,256,value);
        }
        bool alias=false;for(unsigned i=0;i<count;++i)if(records[i].type==5 && !strcasecmp(records[i].owner,current)){
            size_t end=records[i].data;if(!decode(p,n,&end,value) || end!=records[i].data+records[i].length || !value[0])return false;
            lan_text_copy(current,sizeof(current),value);alias=true;break;
        }
        if(!alias)break;
    }return false;
}
size_t posix_address_request(const char *name,uint16_t tx,uint8_t p[512]){
    if(!name || !*name || strlen(name)>253)return 0;memset(p,0,512);p[0]=tx>>8;p[1]=tx;p[2]=1;p[5]=1;size_t used=12;const char *first=name;
    for(const char *s=name;;++s){unsigned c=(unsigned char)*s;if(c && c!='.' && !((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_'))return 0;
        if(c=='.' || !c){size_t bytes=s-first;if(!bytes || bytes>63)return 0;p[used++]=(uint8_t)bytes;memcpy(p+used,first,bytes);used+=bytes;if(!c)break;first=s+1;}}
    p[used++]=0;p[used++]=0;p[used++]=1;p[used++]=0;p[used++]=1;return used;
}
bool posix_address_reply(const uint8_t *p,size_t n,const char *name,uint16_t tx,uint32_t *ip){
    if(n<12 || word(p)!=tx || (word(p+2)&0x800f)!=0x8000 || (word(p+2)&0x7a00) || word(p+4)!=1 || !word(p+6) || word(p+6)>64)return false;
    char current[256],value[256];if(strlen(name)>=sizeof(current))return false;lan_text_copy(current,sizeof(current),name);size_t at=12;if(!decode(p,n,&at,value) || strcasecmp(value,name) || n-at<4 || word(p+at)!=1 || word(p+at+2)!=1)return false;at+=4;
    struct record {char name[256];size_t data;unsigned type,length;} records[64];unsigned count=0;
    for(unsigned i=0;i<word(p+6);++i){if(!decode(p,n,&at,value) || n-at<10)return false;unsigned type=word(p+at),cls=word(p+at+2),bytes=word(p+at+8);at+=10;if(bytes>n-at)return false;if(cls==1){lan_text_copy(records[count].name,sizeof(records[count].name),value);records[count].type=type;records[count].length=bytes;records[count++].data=at;}at+=bytes;}
    for(unsigned step=0;step<64;++step){bool alias=false;for(unsigned i=0;i<count;++i)if(!strcasecmp(records[i].name,current) && records[i].type==1 && records[i].length==4){const uint8_t *a=p+records[i].data;*ip=((uint32_t)a[0]<<24)|((uint32_t)a[1]<<16)|((uint32_t)a[2]<<8)|a[3];return *ip!=0;}
        for(unsigned i=0;i<count;++i)if(!strcasecmp(records[i].name,current) && records[i].type==5){size_t position=records[i].data;if(!decode(p,n,&position,current) || position>records[i].data+records[i].length)return false;alias=true;break;}if(!alias)break;
    }return false;
}
static ssize_t exchange(posix_backend_t *b,uint32_t ip,unsigned port,const uint8_t *request,size_t bytes,uint8_t reply[2048],unsigned timeout){
    if(!posix_pace(b))return -1;int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return -1;struct sockaddr_in source={0},peer={0};source.sin_family=peer.sin_family=AF_INET;source.sin_addr.s_addr=htonl(b->selected.ip);peer.sin_addr.s_addr=htonl(ip);peer.sin_port=htons((uint16_t)port);
    if(bind(fd,(struct sockaddr *)&source,sizeof(source)) || fcntl(fd,F_SETFL,O_NONBLOCK)<0 || fcntl(fd,F_SETFD,FD_CLOEXEC)<0){close(fd);return -1;}
#ifdef __APPLE__
    unsigned index=b->selected.index;/* Local endpoints route through lo0 even when addressed by the NIC IP.
       Keep the source bind, but do not pin them to the physical interface. */
    if(ip!=b->selected.ip && (ip>>24)!=127 && setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index))){close(fd);return -1;}
#elif defined(IP_UNICAST_IF)
    if((ip>>24)!=127){uint32_t index=htonl(b->selected.index);if(setsockopt(fd,IPPROTO_IP,IP_UNICAST_IF,&index,sizeof(index))){close(fd);return -1;}}
#endif
    if(connect(fd,(struct sockaddr *)&peer,sizeof(peer)) || send(fd,request,bytes,0)!=(ssize_t)bytes){close(fd);return -1;}
    uint64_t deadline=now()+timeout;ssize_t received=-1;
    while(!posix_cancelled(b) && now()<deadline){struct pollfd p={fd,POLLIN,0};int ready=poll(&p,1,25);if(ready<0 && errno!=EINTR)break;if(ready>0){received=recv(fd,reply,2048,0);break;}}
    close(fd);return received;
}
bool posix_ptr_lookup(posix_backend_t *b,uint32_t ip,uint32_t server,unsigned port,unsigned timeout,char name[256]){
    uint8_t request[512],reply[2048];uint16_t tx=(uint16_t)(ip^now()^(uintptr_t)pthread_self());size_t n=posix_ptr_request(ip,tx,request);ssize_t bytes=exchange(b,server,port,request,n,reply,timeout);
    return bytes>0 && posix_ptr_reply(reply,(size_t)bytes,ip,tx,name);
}
bool posix_forward(posix_backend_t *b,const char *name,uint32_t *ip){
    uint8_t request[512],reply[2048];uint16_t tx=(uint16_t)(now()^(uintptr_t)pthread_self());size_t n=posix_address_request(name,tx,request);if(!n)return false;
    FILE *hosts=fopen("/etc/hosts","r");if(hosts){char line[1024];size_t read=0;while(read<1024*1024 && !posix_cancelled(b) && fgets(line,sizeof(line),hosts)){read+=strlen(line);char *comment=strchr(line,'#');if(comment)*comment=0;char *save=NULL,*address=strtok_r(line," \t\r\n",&save);uint32_t value;if(!address || !ipv4_parse(address,strlen(address),&value))continue;for(char *alias=strtok_r(NULL," \t\r\n",&save);alias;alias=strtok_r(NULL," \t\r\n",&save))if(!strcasecmp(alias,name)){*ip=value;fclose(hosts);return true;}}fclose(hosts);}
    for(size_t i=0;i<b->selected.dns_count && !posix_cancelled(b);++i){ssize_t bytes=exchange(b,b->selected.dns[i],53,request,n,reply,500);if(bytes>0 && posix_address_reply(reply,(size_t)bytes,name,tx,ip))return true;}return false;
}
void posix_name(posix_backend_t *b,uint32_t ip,char name[256]){
    name[0]=0;uint32_t servers[3];size_t count=b->selected.dns_count;memcpy(servers,b->selected.dns,count*sizeof(*servers));FILE *f=count ? NULL:fopen("/etc/resolv.conf","r");char line[256];
    if(f){while(count<3 && fgets(line,sizeof(line),f)){char s[64];if(sscanf(line,"nameserver %63s",s)==1 && ipv4_parse(s,strlen(s),&servers[count]))++count;}fclose(f);}
    uint8_t request[512],reply[2048];uint16_t tx=(uint16_t)(ip^now()^(uintptr_t)pthread_self());
    for(size_t i=0;i<count && !posix_cancelled(b);++i)if(posix_ptr_lookup(b,ip,servers[i],53,250,name))return;
    if(posix_cancelled(b) || !nbns_request(tx,request,sizeof(request)))return;ssize_t bytes=exchange(b,ip,137,request,50,reply,300);char raw[16];
    if(bytes>0 && nbns_name(reply,(size_t)bytes,tx,raw)){/* ASCII NBSTAT names are unambiguous across OEM code pages. */
        for(size_t i=0;raw[i];++i)if((unsigned char)raw[i]>=128)return;lan_text_copy(name,256,raw);
    }
}
