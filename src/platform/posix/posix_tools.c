#include "posix_tools.h"
#include "posix_names.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __linux__
#include <linux/errqueue.h>
#else
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
extern char **environ;
#endif
struct posix_tools {posix_backend_t backend;pthread_t thread;atomic_bool done;bool joined,trace,forward;char name[256];uint32_t target;unsigned timeout;ping_sample_t samples[64];size_t head,count;};
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void pause_ms(unsigned ms){struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL);}
static void put(posix_tools_t *j,ping_sample_t sample){pthread_mutex_lock(&j->backend.lock);if(j->count<64){j->samples[(j->head+j->count)%64]=sample;++j->count;}pthread_mutex_unlock(&j->backend.lock);}
#ifdef __linux__
static ping_sample_t trace_hop(posix_tools_t *j,unsigned hop){
    ping_sample_t sample={.ip=j->target,.interface_index=j->backend.selected.index,.hop=(uint8_t)hop,.timestamp=(uint64_t)time(NULL),.status=SCAN_TIMEOUT};
    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0){sample.status=SCAN_SYSTEM_ERROR;return sample;}struct sockaddr_in source={0},target={0};source.sin_family=target.sin_family=AF_INET;source.sin_addr.s_addr=htonl(j->backend.selected.ip);target.sin_addr.s_addr=htonl(j->target);target.sin_port=htons((uint16_t)(33434+hop));int enabled=1,ttl=(int)hop;
    if(bind(fd,(struct sockaddr *)&source,sizeof(source)) || setsockopt(fd,IPPROTO_IP,IP_RECVERR,&enabled,sizeof(enabled)) || setsockopt(fd,IPPROTO_IP,IP_TTL,&ttl,sizeof(ttl)) || fcntl(fd,F_SETFL,O_NONBLOCK)<0 || fcntl(fd,F_SETFD,FD_CLOEXEC)<0){sample.status=SCAN_SYSTEM_ERROR;close(fd);return sample;}
    if((j->target>>24)!=127){uint32_t index=htonl(j->backend.selected.index);if(setsockopt(fd,IPPROTO_IP,IP_UNICAST_IF,&index,sizeof(index))){sample.status=SCAN_SYSTEM_ERROR;close(fd);return sample;}}
    if(connect(fd,(struct sockaddr *)&target,sizeof(target))){sample.status=SCAN_SYSTEM_ERROR;close(fd);return sample;}
    const char payload[]="LOOKLAN";uint64_t began=now();if(send(fd,payload,sizeof(payload),0)<0){sample.status=SCAN_NO_ROUTE;close(fd);return sample;}
    while(!atomic_load(&j->backend.cancel) && now()-began<j->timeout){struct pollfd p={fd,POLLERR,0};int result=poll(&p,1,25);if(result<0 && errno!=EINTR)break;if(result<=0)continue;
        char data[64];union {struct cmsghdr alignment;char bytes[256];} control;struct iovec iov={data,sizeof(data)};struct msghdr message={0};message.msg_iov=&iov;message.msg_iovlen=1;message.msg_control=control.bytes;message.msg_controllen=sizeof(control.bytes);
        if(recvmsg(fd,&message,MSG_ERRQUEUE)<0)continue;
        for(struct cmsghdr *c=CMSG_FIRSTHDR(&message);c;c=CMSG_NXTHDR(&message,c))if(c->cmsg_level==IPPROTO_IP && c->cmsg_type==IP_RECVERR && c->cmsg_len>=CMSG_LEN(sizeof(struct sock_extended_err)+sizeof(struct sockaddr_in))){
            struct sock_extended_err *error=(struct sock_extended_err *)CMSG_DATA(c);struct sockaddr_in *peer=(struct sockaddr_in *)SO_EE_OFFENDER(error);
            if(error->ee_origin==SO_EE_ORIGIN_ICMP && peer->sin_family==AF_INET && (error->ee_type==11 || (error->ee_type==3 && error->ee_code==3))){sample.received=true;sample.reply_ip=ntohl(peer->sin_addr.s_addr);sample.latency_ms=(uint32_t)(now()-began);sample.status=error->ee_type==3 && sample.reply_ip==j->target ? SCAN_OK : SCAN_UNREACHABLE;}break;
        }
        break;
    }if(atomic_load(&j->backend.cancel))sample.status=SCAN_CANCELLED;close(fd);return sample;
}
#else
static void mac_trace(posix_tools_t *j){
    char source[32],target[32],timeout[32];ipv4_format(j->backend.selected.ip,source,sizeof(source));ipv4_format(j->target,target,sizeof(target));unsigned wait_seconds=(j->timeout+999)/1000;if(!wait_seconds)wait_seconds=1;snprintf(timeout,sizeof(timeout),"%u",wait_seconds);int pipefd[2];if(pipe(pipefd)){put(j,(ping_sample_t){.ip=j->target,.interface_index=j->backend.selected.index,.status=SCAN_SYSTEM_ERROR});return;}fcntl(pipefd[0],F_SETFD,FD_CLOEXEC);fcntl(pipefd[1],F_SETFD,FD_CLOEXEC);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDOUT_FILENO);posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDERR_FILENO);posix_spawn_file_actions_addclose(&actions,pipefd[0]);posix_spawn_file_actions_addclose(&actions,pipefd[1]);
    char *argv[16]={"/usr/sbin/traceroute","-n","-s",source,"-m","30","-q","1","-w",timeout};unsigned argc=10;if((j->target>>24)!=127){argv[argc++]="-i";argv[argc++]=j->backend.selected.name;}argv[argc++]=target;argv[argc]=NULL;size_t environment_count=0;while(environ[environment_count])++environment_count;char **environment=calloc(environment_count+2,sizeof(*environment));if(!environment){posix_spawn_file_actions_destroy(&actions);close(pipefd[0]);close(pipefd[1]);return;}size_t env_used=0;for(size_t i=0;i<environment_count;++i)if(strncmp(environ[i],"LC_ALL=",7))environment[env_used++]=environ[i];environment[env_used]="LC_ALL=C";pid_t child;int error=posix_spawn(&child,argv[0],&actions,NULL,argv,environment);free(environment);posix_spawn_file_actions_destroy(&actions);close(pipefd[1]);if(error){close(pipefd[0]);put(j,(ping_sample_t){.ip=j->target,.interface_index=j->backend.selected.index,.status=SCAN_SYSTEM_ERROR});return;}fcntl(pipefd[0],F_SETFL,O_NONBLOCK);
    char buffer[4096];size_t used=0;uint64_t deadline=now()+(uint64_t)wait_seconds*30000+5000;bool finished=false;unsigned hops=0;
    while(!atomic_load(&j->backend.cancel) && now()<deadline){char chunk[512];ssize_t n=read(pipefd[0],chunk,sizeof(chunk));if(n>0){for(ssize_t k=0;k<n;++k){if(chunk[k]!='\n'){if(used+1<sizeof(buffer))buffer[used++]=chunk[k];continue;}buffer[used]=0;unsigned hop;char address[64];double ms;int fields=sscanf(buffer," %u %63s %lf",&hop,address,&ms);if(fields>=2 && hop>=1 && hop<=30){uint32_t reply=0;bool received=fields==3 && ipv4_parse(address,strlen(address),&reply);ping_sample_t s={.ip=j->target,.interface_index=j->backend.selected.index,.timestamp=(uint64_t)time(NULL),.hop=(uint8_t)hop,.received=received,.reply_ip=reply,.latency_ms=received && ms>=0 && ms<60000 ? (uint32_t)ms:0,.status=received && reply==j->target ? SCAN_OK : SCAN_TIMEOUT};put(j,s);++hops;}used=0;}}int status;if(!finished && waitpid(child,&status,WNOHANG)==child)finished=true;if(finished && n<=0)break;pause_ms(10);}
    if(!finished){kill(child,SIGKILL);while(waitpid(child,NULL,0)<0 && errno==EINTR){}}close(pipefd[0]);if(!hops && !atomic_load(&j->backend.cancel))put(j,(ping_sample_t){.ip=j->target,.interface_index=j->backend.selected.index,.status=SCAN_SYSTEM_ERROR});
}
#endif
static void *worker(void *context){posix_tools_t *j=context;
    if(j->forward){uint32_t ip=0;bool found=posix_forward(&j->backend,j->name,&ip);put(j,(ping_sample_t){.received=found,.reply_ip=ip,.status=atomic_load(&j->backend.cancel) ? SCAN_CANCELLED:found ? SCAN_OK:SCAN_TIMEOUT});atomic_store(&j->done,true);return NULL;}
    if(j->trace){
#ifdef __linux__
        for(unsigned hop=1;hop<=30 && !atomic_load(&j->backend.cancel);++hop){ping_sample_t sample=trace_hop(j,hop);put(j,sample);if(sample.received && sample.reply_ip==j->target && sample.status==SCAN_OK)break;}
#else
        mac_trace(j);
#endif
    }else for(unsigned i=0;i<4 && !atomic_load(&j->backend.cancel);++i){uint64_t began=now();lan_host_t host={0};scan_result_t result=posix_echo(&j->backend,j->target,j->timeout,&host);ping_sample_t sample={.ip=j->target,.interface_index=j->backend.selected.index,.timestamp=(uint64_t)time(NULL),.received=result==SCAN_OK,.reply_ip=result==SCAN_OK ? j->target:0,.latency_ms=host.latency_ms,.status=(uint32_t)result};put(j,sample);while(i<3 && !atomic_load(&j->backend.cancel) && now()-began<1000)pause_ms(10);}
    atomic_store(&j->done,true);return NULL;
}
posix_tools_t *posix_tools_start(const posix_interface_t *nic,uint32_t target,unsigned timeout,bool trace){posix_tools_t *j=calloc(1,sizeof(*j));if(!j)return NULL;if(!posix_backend_init(&j->backend)){free(j);return NULL;}atomic_init(&j->done,false);j->backend.selected=*nic;j->target=target;j->timeout=timeout;j->trace=trace;if(pthread_create(&j->thread,NULL,worker,j)){pthread_mutex_destroy(&j->backend.lock);free(j);return NULL;}return j;}
posix_tools_t *posix_forward_start(const posix_interface_t *nic,const char *name){uint8_t request[512];if(!posix_address_request(name,1,request))return NULL;posix_tools_t *j=calloc(1,sizeof(*j));if(!j)return NULL;if(!posix_backend_init(&j->backend)){free(j);return NULL;}atomic_init(&j->done,false);j->backend.selected=*nic;j->forward=true;strcpy(j->name,name);if(pthread_create(&j->thread,NULL,worker,j)){pthread_mutex_destroy(&j->backend.lock);free(j);return NULL;}return j;}
void posix_tools_cancel(posix_tools_t *j){if(j)atomic_store(&j->backend.cancel,true);}
bool posix_tools_pop(posix_tools_t *j,ping_sample_t *s){if(!j)return false;pthread_mutex_lock(&j->backend.lock);bool found=j->count!=0;if(found){*s=j->samples[j->head];j->head=(j->head+1)%64;--j->count;}pthread_mutex_unlock(&j->backend.lock);return found;}
bool posix_tools_reap(posix_tools_t *j){if(!atomic_load(&j->done))return false;if(!j->joined){pthread_join(j->thread,NULL);j->joined=true;}return true;}
void posix_tools_free(posix_tools_t *j){if(!j || !j->joined)return;pthread_mutex_destroy(&j->backend.lock);free(j);}
