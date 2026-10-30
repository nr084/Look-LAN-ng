#include "history_store.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
struct history_store {
    char path[4096];profile_t profile;monitor_t model;size_t count;bool load,success;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;atomic_bool done;bool joined;
#endif
};
#ifdef _WIN32
static bool wide(const char *path,wchar_t out[4096]){return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,out,4096)>0;}
#endif
static FILE *open_path(const char *path,bool write){
#ifdef _WIN32
    wchar_t w[4096];return wide(path,w)?_wfopen(w,write?L"wb":L"rb"):NULL;
#else
    return fopen(path,write?"wb":"rb");
#endif
}
bool history_store_exists(const char *path){
#ifdef _WIN32
    wchar_t w[4096];return wide(path,w) && GetFileAttributesW(w)!=INVALID_FILE_ATTRIBUTES;
#else
    return access(path,F_OK)==0;
#endif
}
static bool writer(void *ctx,const char *data,size_t n){return fwrite(data,1,n,ctx)==n;}
static void work(history_store_t *j){
    if(j->load){FILE *f=open_path(j->path,false);if(!f)return;unsigned char *bytes=NULL;
        if(!fseek(f,0,SEEK_END)){long n=ftell(f);if(n>0 && (unsigned long)n<=HISTORY_MAX_BYTES && !fseek(f,0,SEEK_SET)){bytes=malloc((size_t)n);if(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n && !ferror(f))j->success=history_read(bytes,(size_t)n,&j->model,&j->profile,j->model.interface_index);}}
        free(bytes);fclose(f);return;
    }
    char temporary[4096];FILE *f=NULL;
#ifdef _WIN32
    wchar_t target[4096],temp[4096];if(!wide(j->path,target))return;
    swprintf_s(temp,4096,L"%ls.tmp-%lu-%p",target,GetCurrentProcessId(),(void *)j);
    HANDLE handle=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(handle==INVALID_HANDLE_VALUE)return;
    int fd=_open_osfhandle((intptr_t)handle,_O_BINARY);if(fd<0){CloseHandle(handle);DeleteFileW(temp);return;}f=_fdopen(fd,"wb");if(!f){_close(fd);DeleteFileW(temp);return;}
#else
    if(snprintf(temporary,sizeof(temporary),"%s.XXXXXX",j->path)<0 || strlen(j->path)+8>=sizeof(temporary))return;
    int fd=mkstemp(temporary);if(fd<0)return;f=fdopen(fd,"wb");if(!f){close(fd);unlink(temporary);return;}
#endif
    bool ok=history_write(&j->model,&j->profile,writer,f);if(fflush(f))ok=false;
#ifdef _WIN32
    if(_commit(fd))ok=false;
#else
    if(fsync(fd))ok=false;
#endif
    if(fclose(f))ok=false;
#ifdef _WIN32
    j->success=ok && MoveFileExW(temp,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!j->success)DeleteFileW(temp);
#else
    j->success=ok && !rename(temporary,j->path);if(!j->success)unlink(temporary);
#endif
}
#ifdef _WIN32
static DWORD WINAPI worker(void *ctx){work(ctx);return 0;}
#else
static void *worker(void *ctx){history_store_t *j=ctx;work(j);atomic_store(&j->done,true);return NULL;}
#endif
history_store_t *history_store_start(const char *path,const profile_t *p,const monitor_t *m,bool load,uint32_t nic){
    if(!path || strlen(path)+64>=4096 || *m->count>HISTORY_MAX_HOSTS || m->log_count>HISTORY_MAX_EVENTS)return NULL;
    history_store_t *j=calloc(1,sizeof(*j));if(!j)return NULL;strcpy(j->path,path);j->profile=*p;j->load=load;
    size_t hosts=load?m->capacity:*m->count,events=load?m->log_capacity:m->log_count;
    lan_host_t *h=calloc(hosts?hosts:1,sizeof(*h));monitor_event_t *log=calloc(events?events:1,sizeof(*log));
    if(!h || !log){free(h);free(log);free(j);return NULL;}monitor_init(&j->model,h,&j->count,hosts,log,events);j->model.interface_index=nic;
    if(!load){j->count=*m->count;memcpy(h,m->hosts,hosts*sizeof(*h));for(size_t i=0;i<events;++i)log[i]=*monitor_log_at(m,i);j->model.log_count=events;j->model.sequence=m->sequence;j->model.round=m->round;j->model.baseline=m->baseline;}
#ifdef _WIN32
    j->thread=CreateThread(NULL,0,worker,j,0,NULL);if(!j->thread){free(h);free(log);free(j);return NULL;}
#else
    atomic_init(&j->done,false);if(pthread_create(&j->thread,NULL,worker,j)){free(h);free(log);free(j);return NULL;}
#endif
    return j;
}
bool history_store_done(history_store_t *j){
#ifdef _WIN32
    return WaitForSingleObject(j->thread,0)==WAIT_OBJECT_0;
#else
    if(!atomic_load(&j->done))return false;if(!j->joined){pthread_join(j->thread,NULL);j->joined=true;}return true;
#endif
}
bool history_store_success(const history_store_t *j){return j->success;}
void history_store_apply(const history_store_t *j,monitor_t *m){if(!j->load || !j->success || j->count>m->capacity || j->model.log_count>m->log_capacity)return;memcpy(m->hosts,j->model.hosts,j->count*sizeof(*m->hosts));memcpy(m->log,j->model.log,j->model.log_count*sizeof(*m->log));*m->count=j->count;m->log_head=0;m->log_count=j->model.log_count;m->sequence=j->model.sequence;m->round=j->model.round;m->baseline=j->model.baseline;m->interface_index=j->model.interface_index;}
void history_store_free(history_store_t *j){if(!j || !history_store_done(j))return;
#ifdef _WIN32
    CloseHandle(j->thread);
#endif
    free(j->model.hosts);free(j->model.log);free(j);
}
