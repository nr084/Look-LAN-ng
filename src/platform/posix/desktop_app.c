#include "desktop_app.h"
#include "build_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
extern const unsigned char desktop_oui[];
extern const size_t desktop_oui_size;
const char *desktop_tr(const desktop_app_t *a,const char *en,const char *ru){return a->russian ? ru : en;}
static bool ru_system(const char *language){return language && language[0]=='r' && language[1]=='u' && (language[2]=='_' || language[2]=='-' || language[2]==0);}
void desktop_language(desktop_app_t *a,unsigned language,const char *system){
    a->language=language<=2 ? language : 0; a->russian=a->language==2 || (!a->language && ru_system(system));
    if(a->settings[0]){FILE *f=fopen(a->settings,"w");if(f){fprintf(f,"%u\n",a->language);fclose(f);chmod(a->settings,0600);}}
}
bool desktop_init(desktop_app_t *a,const char *system){
    memset(a,0,sizeof(*a));a->hosts=calloc(DESKTOP_HOSTS,sizeof(*a->hosts));a->log=calloc(10000,sizeof(*a->log));
    if(!a->hosts || !a->log || !posix_backend_init(&a->backend)){free(a->hosts);free(a->log);return false;}
    if(!vendor_open(&a->vendors,desktop_oui,desktop_oui_size)){posix_backend_destroy(&a->backend);free(a->hosts);free(a->log);return false;}
    monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);profile_defaults(&a->profile);a->profile.concurrency=32;
    const char *home=getenv("HOME"); if(home){char directory[4096];
#ifdef __APPLE__
        snprintf(directory,sizeof(directory),"%s/Library/Application Support/LookLAN-ng",home);
#else
        const char *config=getenv("XDG_CONFIG_HOME");if(config && config[0]=='/')snprintf(directory,sizeof(directory),"%s",config);else snprintf(directory,sizeof(directory),"%s/.config",home);
        mkdir(directory,0700);size_t n=strlen(directory);snprintf(directory+n,sizeof(directory)-n,"/LookLAN-ng");
#endif
        if(!mkdir(directory,0700) || access(directory,W_OK)==0)snprintf(a->settings,sizeof(a->settings),"%s/language",directory);
    }
    FILE *f=a->settings[0] ? fopen(a->settings,"r") : NULL;unsigned language=0;if(f){if(fscanf(f,"%u",&language)!=1 || language>2)language=0;fclose(f);}
    a->language=language;a->russian=language==2 || (!language && ru_system(system));return desktop_refresh(a);
}
bool desktop_refresh(desktop_app_t *a){if(a->scanning)return false;a->interface_count=posix_interfaces(a->interfaces,POSIX_INTERFACES);a->selected=SIZE_MAX;a->host_count=0;if(a->interface_count)return desktop_select(a,0);return true;}
bool desktop_select(desktop_app_t *a,size_t index){
    if(a->scanning || index>=a->interface_count)return false;a->selected=index;monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);
    posix_interface_t *nic=&a->interfaces[index];profile_defaults(&a->profile);a->profile.concurrency=32;
    snprintf(a->profile.interface_id,sizeof(a->profile.interface_id),"%s",nic->name);network_range(nic->ip,nic->prefix,&a->profile.ranges[0]);a->profile.range_count=1;return true;
}
bool desktop_start(desktop_app_t *a,const char *range){
    if(a->scanning || a->closing || a->selected>=a->interface_count)return false;
    if(range && *range){ipv4_range_t parsed;if(!range_parse(range,strlen(range),&parsed)){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Invalid IPv4 range","Некорректный диапазон IPv4"));return false;}a->profile.ranges[0]=parsed;a->profile.range_count=1;}
    uint64_t addresses=0;if(!ranges_normalize(a->profile.ranges,&a->profile.range_count,&addresses) || addresses>65536 || !addresses){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Use a range of 1–65536 IPv4 addresses","Укажите диапазон из 1–65536 IPv4 адресов"));return false;}
    a->error[0]=0;a->failed=false;a->completed=0;a->total=(uint32_t)addresses;
    if(!posix_scan_start(&a->backend,&a->interfaces[a->selected],&a->profile)){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Cannot start scan","Не удалось начать сканирование"));return false;}
    monitor_begin_ranges(&a->monitor,a->profile.ranges,a->profile.range_count,a->interfaces[a->selected].index,true,(uint64_t)time(NULL));a->scanning=true;return true;
}
bool desktop_poll(desktop_app_t *a){
    posix_event_t event;bool changed=false;unsigned processed=0;
    while(processed++<256 && posix_pop(&a->backend,&event)){
        changed=true;if(event.type==PX_HOST)monitor_observe(&a->monitor,&event.host,true,(uint64_t)time(NULL));
        else if(event.type==PX_PROGRESS){a->completed=event.completed;a->total=event.total;}
        else if(event.type==PX_ERROR){a->failed=true;snprintf(a->error,sizeof(a->error),"Worker error: %d",event.error);}
    }
    pthread_mutex_lock(&a->backend.lock);bool empty=a->backend.count==0;pthread_mutex_unlock(&a->backend.lock);
    if(a->scanning && empty && posix_reap(&a->backend)){monitor_finish(&a->monitor,!a->failed && !atomic_load(&a->backend.cancel),(uint64_t)time(NULL));a->scanning=false;changed=true;}
    return changed;
}
bool desktop_close(desktop_app_t *a){if(a->closed)return true;a->closing=true;posix_stop(&a->backend);if(!posix_backend_destroy(&a->backend))return false;free(a->hosts);free(a->log);a->hosts=NULL;a->log=NULL;a->host_count=0;a->closed=true;return true;}
static void timestamp(uint64_t t,char *out,size_t cap){if(!t){snprintf(out,cap,"—");return;}time_t value=(time_t)t;struct tm local;localtime_r(&value,&local);strftime(out,cap,"%Y-%m-%d %H:%M:%S",&local);}
void desktop_cell(const desktop_app_t *a,size_t index,unsigned col,char *out,size_t cap){
    if(index>=a->host_count){if(cap)out[0]=0;return;}const lan_host_t *h=&a->hosts[index];out[0]=0;
    switch(col){
    case 0:snprintf(out,cap,"%s",h->online ? desktop_tr(a,h->new_device ? "NEW" : "ONLINE",h->new_device ? "НОВЫЙ" : "В СЕТИ") : desktop_tr(a,"OFFLINE","НЕ В СЕТИ"));break;
    case 1:ipv4_format(ipv4_value(&h->address),out,cap);break;
    case 2:snprintf(out,cap,"%s",h->hostname[0] ? h->hostname : desktop_tr(a,"<unknown>","<неизвестно>"));break;
    case 3:if(h->has_mac)snprintf(out,cap,"%02X:%02X:%02X:%02X:%02X:%02X",h->mac[0],h->mac[1],h->mac[2],h->mac[3],h->mac[4],h->mac[5]);break;
    case 4:{const char *vendor=h->has_mac ? vendor_lookup(&a->vendors,h->mac) : NULL;snprintf(out,cap,"%s",vendor ? vendor : "—");break;}
    case 5:snprintf(out,cap,"%s",h->os_family==OS_WINDOWS ? "Windows?" : h->os_family==OS_LINUX ? "Linux / Unix?" : h->os_family==OS_PRINTER ? desktop_tr(a,"Printer?","Принтер?") : "—");break;
    case 6:if(h->has_latency)snprintf(out,cap,"%u ms",h->latency_ms);break;
    case 7:for(unsigned p=0;p<h->port_count;++p){size_t n=strlen(out);if(n+8>=cap)break;snprintf(out+n,cap-n,"%s%u",p ? "," : "",h->open_ports[p]);}break;
    case 8:timestamp(h->first_seen,out,cap);break;case 9:timestamp(h->last_seen,out,cap);break;
    }
}
static bool writer(void *context,const char *data,size_t n){return fwrite(data,1,n,context)==n;}
static FILE *temporary(const char *path,char *temp,size_t cap){if(snprintf(temp,cap,"%s.XXXXXX",path)<0 || strlen(path)+8>=cap)return NULL;int fd=mkstemp(temp);if(fd<0)return NULL;FILE *f=fdopen(fd,"wb");if(!f){close(fd);unlink(temp);}return f;}
static bool finish(FILE *f,const char *temp,const char *path,bool ok){if(fflush(f) || fsync(fileno(f)))ok=false;if(fclose(f))ok=false;if(ok && rename(temp,path))ok=false;if(!ok)unlink(temp);return ok;}
bool desktop_export(const desktop_app_t *a,const char *path,report_format_t format){char temp[4096];FILE *f=temporary(path,temp,sizeof(temp));if(!f)return false;report_options_t options={0};options.russian=a->russian;snprintf(options.profile_name,sizeof(options.profile_name),"%s",a->profile.name);snprintf(options.copyright,sizeof(options.copyright),"© nr084, " APP_BUILD_YEAR);return finish(f,temp,path,report_write(format,a->hosts,a->host_count,&a->vendors,&options,writer,f));}
bool desktop_profile_save(const desktop_app_t *a,const char *path){char temp[4096];FILE *f=temporary(path,temp,sizeof(temp));return f && finish(f,temp,path,profile_write(&a->profile,writer,f));}
bool desktop_profile_open(desktop_app_t *a,const char *path){
    if(a->scanning)return false;FILE *f=fopen(path,"rb");if(!f)return false;
    char *text=malloc(2*1024*1024+1);profile_t *p=malloc(sizeof(*p));if(!text || !p){free(text);free(p);fclose(f);return false;}
    size_t n=fread(text,1,2*1024*1024+1,f);bool ok=!ferror(f) && n<=2*1024*1024 && profile_parse(text,n,p,a->error,sizeof(a->error));fclose(f);
    if(ok){size_t nic=SIZE_MAX;for(size_t i=0;i<a->interface_count;++i)if(!strcmp(a->interfaces[i].name,p->interface_id))nic=i;
        if(nic==SIZE_MAX){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Profile interface unavailable","Интерфейс профиля недоступен"));ok=false;}else{a->profile=*p;a->selected=nic;monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);}}
    free(text);free(p);return ok;
}
int desktop_smoke(const char *language){
    desktop_app_t *a=calloc(1,sizeof(*a));if(!a)return 1;if(!desktop_init(a,language)){free(a);return 2;}
    if(!a->interface_count){desktop_close(a);free(a);return 77;}
    char own[32];ipv4_format(a->interfaces[a->selected].ip,own,sizeof(own));
    bool started=desktop_start(a,own);time_t deadline=time(NULL)+10;
    while(started && a->scanning && time(NULL)<deadline){desktop_poll(a);struct timespec delay={0,25000000};nanosleep(&delay,NULL);}
    bool passed=started && !a->scanning && a->host_count==1 && a->hosts[0].online;
    while(!desktop_close(a)){struct timespec delay={0,25000000};nanosleep(&delay,NULL);}passed=desktop_close(a) && passed;free(a);return passed ? 0 : 3;
}
