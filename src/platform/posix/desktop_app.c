#include "text.h"
#include "desktop_app.h"
#include "build_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <strings.h>
#include "ports.h"
#include "statistics.h"
extern const unsigned char desktop_oui[];
extern const size_t desktop_oui_size;
static uint64_t monotonic_seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec;}
static uint64_t monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
const char *desktop_tr(const desktop_app_t *a,const char *en,const char *ru){return a->russian ? ru : en;}
static bool ru_system(const char *language){return language && language[0]=='r' && language[1]=='u' && (language[2]=='_' || language[2]=='-' || language[2]==0);}
void desktop_language(desktop_app_t *a,unsigned language,const char *system){
    a->language=language<=2 ? language : 0; a->russian=a->language==2 || (!a->language && ru_system(system));
    if(a->settings[0]){FILE *f=fopen(a->settings,"w");if(f){fprintf(f,"%u\n",a->language);fclose(f);chmod(a->settings,0600);}}
}
bool desktop_init(desktop_app_t *a,const char *system){
    memset(a,0,sizeof(*a));a->hosts=calloc(DESKTOP_HOSTS,sizeof(*a->hosts));a->log=calloc(10000,sizeof(*a->log));
    a->details=calloc(1,sizeof(*a->details));
    if(!a->hosts || !a->log || !a->details || !posix_backend_init(&a->backend)){free(a->hosts);free(a->log);free(a->details);return false;}
    if(!vendor_open(&a->vendors,desktop_oui,desktop_oui_size)){posix_backend_destroy(&a->backend);free(a->hosts);free(a->log);free(a->details);return false;}
    monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);profile_defaults(&a->profile);
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
    if(a->settings[0]){snprintf(a->profiles_directory,sizeof(a->profiles_directory),"%s",a->settings);char *slash=strrchr(a->profiles_directory,'/');if(slash && strlen(a->profiles_directory)+2<sizeof(a->profiles_directory))snprintf(slash,sizeof(a->profiles_directory)-(size_t)(slash-a->profiles_directory),"/profiles");else a->profiles_directory[0]=0;}
    a->language=language;a->russian=language==2 || (!language && ru_system(system));return desktop_refresh(a);
}
bool desktop_refresh(desktop_app_t *a){if(a->scanning || a->monitoring || a->tools || a->io)return false;a->interface_count=posix_interfaces(a->interfaces,POSIX_INTERFACES);a->selected=SIZE_MAX;a->host_count=0;size_t best=SIZE_MAX;for(size_t i=0;i<a->interface_count;++i){const posix_interface_t *nic=&a->interfaces[i];if(!nic->automatic)continue;if(best==SIZE_MAX || (nic->gateway && !a->interfaces[best].gateway) || (nic->gateway && nic->has_metric && a->interfaces[best].has_metric && nic->metric<a->interfaces[best].metric))best=i;}return best!=SIZE_MAX ? desktop_select(a,best):true;}
bool desktop_select(desktop_app_t *a,size_t index){
    if(a->scanning || a->monitoring || a->tools || a->io || index>=a->interface_count)return false;a->selected=index;monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);
    a->completed=a->total=0;a->scan_started_ms=a->scan_elapsed_ms=0;
    posix_invalidate_names(&a->backend);memset(a->arp_only,0,sizeof(a->arp_only));
    posix_interface_t *nic=&a->interfaces[index];profile_defaults(&a->profile);
    snprintf(a->profile.interface_id,sizeof(a->profile.interface_id),"%s",nic->name);network_range(nic->ip,nic->prefix,&a->profile.ranges[0]);a->profile.range_count=1;return true;
}
bool desktop_start(desktop_app_t *a,const char *range){
    if(a->scanning || a->closing || a->tools || a->io || a->selected>=a->interface_count)return false;
    if(range && *range){ipv4_range_t parsed;if(!range_parse(range,strlen(range),&parsed)){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Invalid IPv4 range","Некорректный диапазон IPv4"));return false;}a->profile.ranges[0]=parsed;a->profile.range_count=1;}
    uint64_t addresses=0;if(!ranges_normalize(a->profile.ranges,&a->profile.range_count,&addresses) || addresses>65536 || !addresses){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Use a range of 1–65536 IPv4 addresses","Укажите диапазон из 1–65536 IPv4 адресов"));return false;}
    a->error[0]=0;a->failed=false;a->completed=0;a->total=(uint32_t)addresses;
    memset(a->round_proof,0,sizeof(a->round_proof));memset(a->cached_round,0,sizeof(a->cached_round));
    a->backend.pace_ms=a->monitoring ? 16:4;a->backend.monitoring=a->monitoring;
    if(!posix_scan_start(&a->backend,&a->interfaces[a->selected],&a->profile)){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Cannot start scan","Не удалось начать сканирование"));return false;}
    monitor_begin_ranges(&a->monitor,a->profile.ranges,a->profile.range_count,a->interfaces[a->selected].index,true,(uint64_t)time(NULL));a->scan_started_ms=monotonic_ms();a->scan_elapsed_ms=0;a->status_updated_ms=a->scan_started_ms;a->scanning=true;return true;
}
uint64_t desktop_scope_size(const desktop_app_t *a,const char *range){if(range && *range){ipv4_range_t value;return range_parse(range,strlen(range),&value) ? (uint64_t)value.last-value.first+1:0;}uint64_t total=0;for(size_t i=0;i<a->profile.range_count;++i)total+=(uint64_t)a->profile.ranges[i].last-a->profile.ranges[i].first+1;return total;}
bool desktop_single(desktop_app_t *a,uint32_t ip){ipv4_range_t ranges[NETWORK_MAX_RANGES];size_t count=a->profile.range_count;memcpy(ranges,a->profile.ranges,count*sizeof(*ranges));char target[32];ipv4_format(ip,target,sizeof(target));bool started=desktop_start(a,target);memcpy(a->profile.ranges,ranges,count*sizeof(*ranges));a->profile.range_count=count;if(started){lan_host_t candidate={0};candidate.address=ipv4_address(ip);candidate.interface_index=a->interfaces[a->selected].index;candidate.cache_only=true;monitor_observe(&a->monitor,&candidate,false,(uint64_t)time(NULL));}return started;}
bool desktop_single_target(desktop_app_t *a,const char *target){if(a->scanning || a->monitoring || a->tools || a->io || a->closing || a->selected>=a->interface_count)return false;uint32_t ip;if(ipv4_parse(target,strlen(target),&ip))return desktop_single(a,ip);a->tools=posix_forward_start(&a->interfaces[a->selected],target);if(!a->tools){snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Invalid host name or IPv4 address","Некорректное имя узла или адрес IPv4"));return false;}a->resolving=true;a->resolve_cancelled=false;a->resolved_ip=0;return true;}
bool desktop_poll(desktop_app_t *a){
    posix_event_t event;bool changed=false;unsigned processed=0;
    if(a->io && desktop_io_finished(a->io)){
        desktop_io_kind_t kind=desktop_io_kind(a->io);if(kind==DESKTOP_LIST_PROFILES){a->saved_count=desktop_io_profiles(a->io,a->saved_profiles);a->saved_changed=true;}profile_t *p=kind==DESKTOP_OPEN_PROFILE ? malloc(sizeof(*p)):NULL;bool cancelled=false;bool success=desktop_io_result(a->io,p,&cancelled);desktop_io_free(a->io);a->io=NULL;
        if(kind==DESKTOP_OPEN_PROFILE && success && p){size_t nic=SIZE_MAX;for(size_t i=0;i<a->interface_count;++i)if(!strcmp(a->interfaces[i].name,p->interface_id))nic=i;if(nic!=SIZE_MAX){posix_invalidate_names(&a->backend);memset(a->arp_only,0,sizeof(a->arp_only));a->profile=*p;a->selected=nic;monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);a->profile_changed=true;}else{free(a->pending_profile);a->pending_profile=p;p=NULL;}}
        if(kind==DESKTOP_OPEN_PROFILE && !p && !a->pending_profile)success=false;free(p);
        if(kind==DESKTOP_EXPORT)monitor_note(&a->monitor,success ? MON_EXPORT_FINISHED:MON_EXPORT_ERROR,cancelled ? "Cancelled":NULL,(uint64_t)time(NULL));
        if(!success && !cancelled && !a->error[0])snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"File operation failed","Не удалось выполнить операцию с файлом"));changed=true;
        if(kind==DESKTOP_SAVE_PROFILE && success && a->profiles_directory[0])desktop_reload_profiles(a);
    }
    if(a->tools){ping_sample_t sample;while(posix_tools_pop(a->tools,&sample)){if(a->resolving){if(sample.received && sample.status!=SCAN_CANCELLED)a->resolved_ip=sample.reply_ip;}else if(a->tracing){if(a->trace_count<30)a->trace[a->trace_count++]=sample;}else details_ping_add(a->details,sample);changed=true;}
        if(posix_tools_reap(a->tools)){while(posix_tools_pop(a->tools,&sample)){if(a->resolving){if(sample.received && sample.status!=SCAN_CANCELLED)a->resolved_ip=sample.reply_ip;}else if(a->tracing){if(a->trace_count<30)a->trace[a->trace_count++]=sample;}else details_ping_add(a->details,sample);}posix_tools_free(a->tools);a->tools=NULL;if(a->resolving){a->resolving=false;if(!a->resolve_cancelled){if(a->resolved_ip)desktop_single(a,a->resolved_ip);else snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"No IPv4 address found","IPv4-адрес не найден"));}}changed=true;}}
    while(processed++<256 && posix_pop(&a->backend,&event)){
        if(event.generation!=atomic_load(&a->backend.generation))continue;
        changed=true;if(event.type==PX_HOST){size_t index=monitor_observe(&a->monitor,&event.host,true,(uint64_t)time(NULL));if(index<DESKTOP_HOSTS){a->round_proof[index]|=(uint8_t)event.proof;a->row_dirty[index]=true;}}
        else if(event.type==PX_CACHE){size_t index=monitor_observe(&a->monitor,&event.host,false,(uint64_t)time(NULL));if(index<DESKTOP_HOSTS){a->cached_round[index]=event.host.has_mac;a->row_dirty[index]=true;}}
        else if(event.type==PX_NAME){bool stale=false;for(size_t i=0;event.host.has_mac && i<a->host_count;++i)if(a->hosts[i].interface_index==event.host.interface_index && !memcmp(&a->hosts[i].address,&event.host.address,sizeof(event.host.address)) && a->hosts[i].has_mac && memcmp(a->hosts[i].mac,event.host.mac,6)){stale=true;break;}if(stale)continue;size_t index=monitor_observe(&a->monitor,&event.host,false,(uint64_t)time(NULL));if(index<DESKTOP_HOSTS)a->row_dirty[index]=true;}
        else if(event.type==PX_PING){details_ping_add(a->details,(ping_sample_t){.ip=ipv4_value(&event.host.address),.interface_index=a->interfaces[a->selected].index,.timestamp=(uint64_t)time(NULL),.latency_ms=event.host.latency_ms,.received=event.error==SCAN_OK,.reply_ip=event.error==SCAN_OK ? ipv4_value(&event.host.address):0,.status=(uint32_t)event.error});}
        else if(event.type==PX_PROGRESS){a->completed=event.completed;a->total=event.total;}
        else if(event.type==PX_ERROR){a->failed=true;snprintf(a->error,sizeof(a->error),"%s: %d",desktop_tr(a,"Network error","Ошибка сети"),event.error);monitor_note(&a->monitor,MON_SCAN_ERROR,a->error,(uint64_t)time(NULL));}
    }
    pthread_mutex_lock(&a->backend.lock);bool empty=a->backend.count==0;pthread_mutex_unlock(&a->backend.lock);
    if(a->scanning && empty && posix_reap(&a->backend)){bool complete=!a->failed && !atomic_load(&a->backend.cancel);if(complete)for(size_t i=0;i<a->host_count;++i){lan_host_t *h=&a->hosts[i];if(a->round_proof[i])a->arp_only[i]=a->round_proof[i]==PX_PROOF_ARP;else if(a->arp_only[i] && h->first_seen && h->seen_round!=a->monitor.round){h->cache_only=a->cached_round[i];a->row_dirty[i]=true;if(h->cache_only){h->online=false;h->has_latency=false;}else if(h->missed_rounds+1>=a->monitor.offline_threshold)h->online=true;}}monitor_finish(&a->monitor,complete,(uint64_t)time(NULL));if(complete)for(size_t i=0;i<a->host_count;++i)if(!a->hosts[i].online)a->row_dirty[i]=true;a->scan_elapsed_ms=monotonic_ms()-a->scan_started_ms;a->scanning=false;if(a->failed)a->monitoring=false;a->next_round=monotonic_seconds()+a->profile.monitor_interval_sec;changed=true;}
    if(a->monitoring && !a->scanning && !a->tools && !a->io && monotonic_seconds()>=a->next_round){if(!desktop_start(a,NULL))a->monitoring=false;changed=true;}
    if(a->scanning && monotonic_ms()-a->status_updated_ms>=150){a->status_updated_ms=monotonic_ms();changed=true;}
    bool names_pending=posix_names_pending(&a->backend);if(names_pending!=a->names_pending){a->names_pending=names_pending;changed=true;}return changed;
}
bool desktop_close(desktop_app_t *a){if(a->closed)return true;a->closing=true;desktop_stop(a);if(a->io){if(!desktop_io_finished(a->io))return false;desktop_io_result(a->io,NULL,NULL);desktop_io_free(a->io);a->io=NULL;}if(a->tools){if(!posix_tools_reap(a->tools))return false;posix_tools_free(a->tools);a->tools=NULL;}if(!posix_backend_destroy(&a->backend))return false;free(a->pending_profile);a->pending_profile=NULL;free(a->hosts);free(a->log);free(a->details);a->hosts=NULL;a->log=NULL;a->details=NULL;a->host_count=0;a->closed=true;return true;}
static void timestamp(uint64_t t,char *out,size_t cap){if(!t){snprintf(out,cap,"—");return;}time_t value=(time_t)t;struct tm local;localtime_r(&value,&local);strftime(out,cap,"%Y-%m-%d %H:%M:%S",&local);}
void desktop_cell(const desktop_app_t *a,size_t index,unsigned col,char *out,size_t cap){
    if(index>=a->host_count){if(cap)out[0]=0;return;}const lan_host_t *h=&a->hosts[index];out[0]=0;
    switch(col){
    case 0:snprintf(out,cap,"%s",!h->first_seen || h->cache_only ? desktop_tr(a,"UNKNOWN","НЕИЗВЕСТНО") : !h->online ? desktop_tr(a,"OFFLINE","НЕ В СЕТИ") : h->new_device ? desktop_tr(a,"NEW","НОВЫЙ") : h->changed ? desktop_tr(a,"CHANGED","ИЗМЕНЁН") : h->online ? desktop_tr(a,"ONLINE","В СЕТИ") : desktop_tr(a,"OFFLINE","НЕ В СЕТИ"));break;
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
bool desktop_export(desktop_app_t *a,const char *path,report_format_t format){if(a->io)return false;a->io=desktop_io_start(DESKTOP_EXPORT,path,&a->profile,a->hosts,a->host_count,&a->vendors,format,a->russian,NULL);if(a->io)monitor_note(&a->monitor,MON_EXPORT_STARTED,path,(uint64_t)time(NULL));return a->io!=NULL;}
bool desktop_profile_save(desktop_app_t *a,const char *path){if(a->io)return false;a->io=desktop_io_start(DESKTOP_SAVE_PROFILE,path,&a->profile,NULL,0,&a->vendors,REPORT_CSV,a->russian,a->profiles_directory);return a->io!=NULL;}
bool desktop_reload_profiles(desktop_app_t *a){if(a->io || !a->profiles_directory[0])return false;a->io=desktop_io_start(DESKTOP_LIST_PROFILES,a->profiles_directory,&a->profile,NULL,0,&a->vendors,REPORT_CSV,a->russian,NULL);return a->io!=NULL;}
bool desktop_profile_rebind(desktop_app_t *a,size_t index){if(!a->pending_profile)return false;if(index>=a->interface_count){free(a->pending_profile);a->pending_profile=NULL;return false;}if(a->scanning || a->monitoring || a->tools || a->io)return false;posix_invalidate_names(&a->backend);a->profile=*a->pending_profile;free(a->pending_profile);a->pending_profile=NULL;a->selected=index;snprintf(a->profile.interface_id,sizeof(a->profile.interface_id),"%s",a->interfaces[index].name);memset(a->arp_only,0,sizeof(a->arp_only));monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);a->profile_changed=true;return true;}
bool desktop_profile_open(desktop_app_t *a,const char *path){if(a->io || a->scanning || a->monitoring || a->tools)return false;a->io=desktop_io_start(DESKTOP_OPEN_PROFILE,path,&a->profile,NULL,0,&a->vendors,REPORT_CSV,a->russian,NULL);return a->io!=NULL;}
void desktop_stop(desktop_app_t *a){a->monitoring=false;if(a->resolving)a->resolve_cancelled=true;posix_stop(&a->backend);posix_tools_cancel(a->tools);desktop_io_cancel(a->io);}
bool desktop_diagnostic(desktop_app_t *a,uint32_t ip,bool trace){if(a->scanning || a->tools || a->io || a->closing || a->selected>=a->interface_count)return false;a->tools=posix_tools_start(&a->interfaces[a->selected],ip,a->profile.icmp_timeout_ms,trace);if(!a->tools)return false;a->tracing=trace;if(trace){a->trace_count=0;a->trace_ip=ip;a->trace_interface=a->interfaces[a->selected].index;}return true;}
static const char *diagnostic_status(const desktop_app_t *a,uint32_t status){
    switch(status){
    case SCAN_OK:return "OK";
    case SCAN_TIMEOUT:return desktop_tr(a,"Timed out","Нет ответа");
    case SCAN_UNREACHABLE:return desktop_tr(a,"Unreachable","Недоступен");
    case SCAN_REFUSED:return desktop_tr(a,"Refused","Соединение отклонено");
    case SCAN_NO_ROUTE:return desktop_tr(a,"No route","Нет маршрута");
    case SCAN_CANCELLED:return desktop_tr(a,"Cancelled","Отменено");
    default:return desktop_tr(a,"System error","Системная ошибка");
    }
}
static size_t recent_ping(const desktop_app_t *a,uint32_t ip,uint32_t scope,size_t indices[60]){
    size_t count=0;for(size_t i=a->details->ping_count;i && count<60;--i){
        const ping_sample_t *sample=details_ping_at(a->details,i-1);
        if(sample->ip==ip && sample->interface_index==scope)indices[count++]=i-1;
    }return count;
}
void desktop_detail(const desktop_app_t *a,size_t index,char *out,size_t capacity){
    if(!capacity)return;out[0]=0;if(index>=a->host_count)return;const lan_host_t *h=&a->hosts[index];uint32_t ip=ipv4_value(&h->address);size_t used=0;const char *en[]={"Status","IP Address","Host Name","MAC Address","Vendor","Device / OS","Latency","Services","First Seen","Last Seen"},*ru[]={"Статус","IP-адрес","Имя узла","MAC-адрес","Производитель","Устройство / ОС","Задержка","Сервисы","Первый ответ","Последний ответ"};
    for(unsigned c=0;c<10;++c){char value[512];desktop_cell(a,index,c,value,sizeof(value));int n=snprintf(out+used,capacity-used,"%s: %s\n",a->russian ? ru[c]:en[c],value);if(n<0 || (size_t)n>=capacity-used)return;used+=n;}
    int n=snprintf(out+used,capacity-used,"\n%s: %s\n\nPing\n",desktop_tr(a,"Notes","Заметки"),details_note(a->details,ip,h->interface_index));if(n<0 || (size_t)n>=capacity-used)return;used+=n;
    char value[8192];desktop_detail_part(a,index,2,value,sizeof(value));
    n=snprintf(out+used,capacity-used,"%s",value);if(n<0 || (size_t)n>=capacity-used)return;used+=n;
    if(a->trace_ip==ip && a->trace_interface==h->interface_index){desktop_detail_part(a,index,3,value,sizeof(value));snprintf(out+used,capacity-used,"\nTrace\n%s",value);}

}
void desktop_network_text(const desktop_app_t *a,char *out,size_t capacity){if(a->selected>=a->interface_count){snprintf(out,capacity,"%s",desktop_tr(a,"Select a network interface","Выберите сетевой интерфейс"));return;}const posix_interface_t *nic=&a->interfaces[a->selected];char ip[32],gateway[32],dns[128]="",metric[32];ipv4_format(nic->ip,ip,sizeof(ip));if(nic->gateway)ipv4_format(nic->gateway,gateway,sizeof(gateway));else snprintf(gateway,sizeof(gateway),"—");if(nic->has_metric)snprintf(metric,sizeof(metric),"%u",nic->metric);else snprintf(metric,sizeof(metric),"—");for(size_t i=0;i<nic->dns_count;++i){char value[32];ipv4_format(nic->dns[i],value,sizeof(value));size_t n=strlen(dns);snprintf(dns+n,sizeof(dns)-n,"%s%s",i ? ", ":"",value);}snprintf(out,capacity,"%s | %s/%u | %s: %s | DNS: %s | %s: %s | %s: %s",nic->name,ip,nic->prefix,desktop_tr(a,"Gateway","Шлюз"),gateway,dns[0] ? dns:"—",desktop_tr(a,"Metric","Метрика"),metric,desktop_tr(a,"Profile","Профиль"),a->profile.name);}
void desktop_status_text(const desktop_app_t *a,char *out,size_t capacity){
    statistics_t stats=statistics_compute(a->hosts,a->host_count,a->completed);
    const char *state=desktop_tr(a,a->scanning ? "Scanning":a->tools ? "Diagnostics":a->io ? "File operation":a->monitoring ? "Monitoring":a->names_pending ? "Names":"Ready",a->scanning ? "Сканирование":a->tools ? "Диагностика":a->io ? "Работа с файлом":a->monitoring ? "Мониторинг":a->names_pending ? "Имена":"Готово");
    uint64_t elapsed=a->scanning && a->scan_started_ms ? monotonic_ms()-a->scan_started_ms:a->scan_elapsed_ms;
    snprintf(out,capacity,"%s | %s: %zu | %s: %zu | %s: %u / %u | %llu.%03llu %s",state,desktop_tr(a,"Online","В сети"),stats.online,desktop_tr(a,"Offline","Не в сети"),stats.offline,desktop_tr(a,"Scanned","Проверено"),a->completed,a->total,(unsigned long long)(elapsed/1000),(unsigned long long)(elapsed%1000),desktop_tr(a,"s","с"));
}
static bool append_detail(char *out,size_t capacity,size_t *used,const char *format,...){if(*used>=capacity)return false;va_list args;va_start(args,format);int n=vsnprintf(out+*used,capacity-*used,format,args);va_end(args);if(n<0 || (size_t)n>=capacity-*used){out[*used]=0;return false;}*used+=(size_t)n;return true;}
void desktop_detail_part(const desktop_app_t *a,size_t index,unsigned part,char *out,size_t capacity){
    if(!capacity)return;out[0]=0;if(part==5){desktop_history(a,out,capacity);return;}if(index>=a->host_count)return;const lan_host_t *h=&a->hosts[index];uint32_t ip=ipv4_value(&h->address);size_t used=0;
    if(part==0){desktop_detail(a,index,out,capacity);char *end=strstr(out,"\n\nPing\n");if(end)*end=0;used=strlen(out);const char *name="—";for(size_t i=0;i<a->interface_count;++i)if(a->interfaces[i].index==h->interface_index)name=a->interfaces[i].name;char ttl[16];if(h->ttl)snprintf(ttl,sizeof(ttl),"%u",h->ttl);else snprintf(ttl,sizeof(ttl),"—");append_detail(out,capacity,&used,"\n%s: %s\nTTL: %s\n",desktop_tr(a,"Interface","Интерфейс"),name,ttl);return;}
    if(part==1){for(unsigned i=0;i<h->port_count;++i)if(!append_detail(out,capacity,&used,"%u  %s\n",h->open_ports[i],port_service(h->open_ports[i])))break;if(h->ports_truncated)append_detail(out,capacity,&used,"\n%s\n",desktop_tr(a,"Only the first 64 open ports are shown","Показаны первые 64 открытых порта"));if(h->ports_checked && !h->port_count)append_detail(out,capacity,&used,"%s",desktop_tr(a,"No open ports found","Открытые порты не найдены"));if(!h->ports_checked)append_detail(out,capacity,&used,"%s",desktop_tr(a,"Ports not checked","Порты не проверены"));return;}
    if(part==2){
        size_t samples[60],count=recent_ping(a,ip,h->interface_index,samples);
        for(size_t i=count;i;--i){const ping_sample_t *sample=details_ping_at(a->details,samples[i-1]);char date[32];timestamp(sample->timestamp,date,sizeof(date));
            bool ok=sample->received ? append_detail(out,capacity,&used,"%s  OK  %u ms\n",date,sample->latency_ms):append_detail(out,capacity,&used,"%s  %s\n",date,diagnostic_status(a,sample->status));if(!ok)break;
        }if(!count)append_detail(out,capacity,&used,"%s",desktop_tr(a,"Use Ping to request replies","Нажмите Ping для проверки ответов"));return;
    }
    if(part==3){
        if(a->trace_ip!=ip || a->trace_interface!=h->interface_index || !a->trace_count){append_detail(out,capacity,&used,"%s",desktop_tr(a,"Use Traceroute to request the route","Нажмите Traceroute для проверки маршрута"));return;}
        for(size_t i=0;i<a->trace_count;++i){const ping_sample_t *sample=&a->trace[i];bool ok;
            if(sample->received){char reply[32];ipv4_format(sample->reply_ip,reply,sizeof(reply));ok=append_detail(out,capacity,&used,"%u  %s  %u ms\n",sample->hop,reply,sample->latency_ms);}
            else if(sample->hop)ok=append_detail(out,capacity,&used,"%u  *  %s\n",sample->hop,diagnostic_status(a,sample->status));
            else ok=append_detail(out,capacity,&used,"%s\n",diagnostic_status(a,sample->status));
            if(!ok)break;
        }return;
    }
    if(part==4){
        size_t events[200],count=0;
        for(size_t i=a->monitor.log_count;i && count<200;--i){const monitor_event_t *event=monitor_log_at(&a->monitor,i-1);if(event->address.family==4 && ipv4_value(&event->address)==ip)events[count++]=i-1;}
        for(size_t i=count;i;--i){char date[64],event[128],value[600];size_t row=events[i-1];desktop_log_cell(a,row,0,date,sizeof(date));desktop_log_cell(a,row,1,event,sizeof(event));desktop_log_cell(a,row,3,value,sizeof(value));if(!append_detail(out,capacity,&used,"%s  %s  %s\n",date,event,value))break;}
        if(!count)append_detail(out,capacity,&used,"%s",desktop_tr(a,"No changes in this session","В этой сессии изменений нет"));
    }

}
int desktop_compare(const desktop_app_t *a,size_t first,size_t second,unsigned col){
    if(first>=a->host_count || second>=a->host_count)return 0;const lan_host_t *x=&a->hosts[first],*y=&a->hosts[second];uint64_t u=0,v=0;bool numeric=true;
    if(col==1){u=ipv4_value(&x->address);v=ipv4_value(&y->address);}else if(col==6){u=x->has_latency ? x->latency_ms:UINT64_MAX;v=y->has_latency ? y->latency_ms:UINT64_MAX;}else if(col==8){u=x->first_seen;v=y->first_seen;}else if(col==9){u=x->last_seen;v=y->last_seen;}else numeric=false;
    int order;if(numeric)order=(u>v)-(u<v);else{char a_text[512],b_text[512];desktop_cell(a,first,col,a_text,sizeof(a_text));desktop_cell(a,second,col,b_text,sizeof(b_text));order=strcasecmp(a_text,b_text);}if(!order && col!=1){u=ipv4_value(&x->address);v=ipv4_value(&y->address);order=(u>v)-(u<v);}return order;
}
bool desktop_monitor(desktop_app_t *a,const char *range){
    if(a->monitoring){desktop_stop(a);return true;}
    if(!desktop_start(a,range))return false;a->monitoring=true;return true;
}
bool desktop_profile_configure(desktop_app_t *a,const char *name,const char *ranges,const char *ports,unsigned icmp,unsigned tcp_timeout,unsigned concurrency,unsigned interval,const char *notes){
    if(a->closing || a->scanning || a->monitoring || a->tools || a->io || a->selected>=a->interface_count)return false;
    profile_t *p=malloc(sizeof(*p));if(!p)return false;*p=a->profile;bool ok=name && notes && ranges && ports && strlen(name)<sizeof(p->name) && strlen(notes)<sizeof(p->notes);
    a->error[0]=0;
    if(ok){lan_text_copy(p->name,sizeof(p->name),name);lan_text_copy(p->notes,sizeof(p->notes),notes);p->range_count=0;
        const char *first=ranges;for(const char *s=ranges;;++s)if(!*s || *s=='\n' || *s==';' || *s==','){
            if(s>first){if(p->range_count==NETWORK_MAX_RANGES || !range_parse(first,(size_t)(s-first),&p->ranges[p->range_count])){ok=false;break;}++p->range_count;}
            if(!*s)break;first=s+1;
        }
        p->icmp_timeout_ms=icmp;p->tcp_timeout_ms=tcp_timeout;p->concurrency=concurrency;p->monitor_interval_sec=interval;
        ok=ok && ports_parse(ports,strlen(ports),p->ports,65535,&p->port_count) && profile_validate(p,NULL,0);
        uint64_t total=0;ok=ok && ranges_normalize(p->ranges,&p->range_count,&total) && total<=65536;
    }
    if(ok){posix_invalidate_names(&a->backend);a->profile=*p;memset(a->arp_only,0,sizeof(a->arp_only));monitor_init(&a->monitor,a->hosts,&a->host_count,DESKTOP_HOSTS,a->log,10000);a->error[0]=0;}
    else if(!a->error[0])snprintf(a->error,sizeof(a->error),"%s",desktop_tr(a,"Invalid profile settings","Некорректные параметры профиля"));
    free(p);return ok;
}
void desktop_statistics_text(const desktop_app_t *a,char *out,size_t capacity){statistics_t s=statistics_compute(a->hosts,a->host_count,a->completed);snprintf(out,capacity,"%s: %u\n%s: %zu\n%s: %zu\n%s: %zu\n%s: %zu\n%s: %zu\n\nWindows: %zu\nLinux / Unix: %zu\n%s: %zu\n%s: %zu\n",desktop_tr(a,"Scanned","Проверено"),s.scanned,desktop_tr(a,"Hosts","Узлы"),s.records,desktop_tr(a,"Online","В сети"),s.online,desktop_tr(a,"Offline","Не в сети"),s.offline,desktop_tr(a,"Unconfirmed","Неподтверждённые"),s.unknown,desktop_tr(a,"New","Новые"),s.fresh,s.windows,s.unix_hosts,desktop_tr(a,"Network devices","Сетевые устройства"),s.network_devices,desktop_tr(a,"Unknown OS","Неизвестная ОС"),s.unknown_os);}
void desktop_log_cell(const desktop_app_t *a,size_t row,unsigned column,char *out,size_t capacity){
    if(!capacity)return;out[0]=0;const monitor_event_t *e=monitor_log_at(&a->monitor,row);if(!e)return;
    const char *en[]={"Scan started","Scan finished","Scan cancelled","ONLINE","OFFLINE","NEW","IP changed","MAC changed","Name changed","Scan error","Export started","Export finished","Export error"},*ru[]={"Начало сканирования","Сканирование завершено","Сканирование отменено","В СЕТИ","НЕ В СЕТИ","НОВЫЙ","Изменён IP","Изменён MAC","Изменено имя","Ошибка сканирования","Начало экспорта","Экспорт завершён","Ошибка экспорта"};
    if(column==0)timestamp(e->timestamp,out,capacity);
    else if(column==1)snprintf(out,capacity,"%s",(unsigned)e->type<13 ? (a->russian ? ru[e->type]:en[e->type]):"?");
    else if(column==2 && e->host_index!=SIZE_MAX)ipv4_format(ipv4_value(&e->address),out,capacity);
    else if(column==3)snprintf(out,capacity,"%s%s%s",e->previous,e->previous[0] ? " → ":"",e->current);
}
void desktop_history(const desktop_app_t *a,char *out,size_t capacity){
    if(!capacity)return;out[0]=0;size_t used=0;
    const char *en[]={"Scan started","Scan finished","Scan cancelled","ONLINE","OFFLINE","NEW","IP changed","MAC changed","Name changed","Scan error","Export started","Export finished","Export error"};
    const char *ru[]={"Начало сканирования","Сканирование завершено","Сканирование отменено","В СЕТИ","НЕ В СЕТИ","НОВЫЙ","Изменён IP","Изменён MAC","Изменено имя","Ошибка сканирования","Начало экспорта","Экспорт завершён","Ошибка экспорта"};
    size_t start=a->monitor.log_count>200 ? a->monitor.log_count-200 : 0;
    for(size_t i=start;i<a->monitor.log_count;++i){const monitor_event_t *e=monitor_log_at(&a->monitor,i);char date[32],ip[32];timestamp(e->timestamp,date,sizeof(date));ipv4_format(ipv4_value(&e->address),ip,sizeof(ip));
        int n=snprintf(out+used,capacity-used,"%s  %s  %s  %s%s%s\n",date,(unsigned)e->type<13 ? (a->russian ? ru[e->type] : en[e->type]) : "?",e->host_index==SIZE_MAX ? "" : ip,e->previous,e->previous[0] ? " → " : "",e->current);
        if(n<0 || (size_t)n>=capacity-used)break;used+=(size_t)n;
    }
}
int desktop_smoke(const char *language){
    desktop_app_t *a=calloc(1,sizeof(*a));if(!a)return 1;if(!desktop_init(a,language)){free(a);return 2;}
    if(a->selected>=a->interface_count){desktop_close(a);free(a);return 77;}
    char own[32];ipv4_format(a->interfaces[a->selected].ip,own,sizeof(own));
    bool started=desktop_start(a,own);time_t deadline=time(NULL)+10;
    while(started && a->scanning && time(NULL)<deadline){desktop_poll(a);struct timespec delay={0,25000000};nanosleep(&delay,NULL);}
    bool passed=started && !a->scanning && a->host_count==1 && a->hosts[0].online;
    while(!desktop_close(a)){struct timespec delay={0,25000000};nanosleep(&delay,NULL);}passed=desktop_close(a) && passed;free(a);return passed ? 0 : 3;
}
