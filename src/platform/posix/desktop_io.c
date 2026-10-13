#include "text.h"
#include "desktop_io.h"
#include "build_info.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
struct desktop_io {pthread_t thread;atomic_bool cancel,done;bool success,joined;desktop_io_kind_t kind;char path[4096],managed[4096];profile_t *profile;lan_host_t *hosts;size_t count;vendor_table_t vendors;report_format_t format;report_options_t options;FILE *file;desktop_saved_profile_t saved[64];size_t saved_count;};
static bool writer(void *context,const char *data,size_t n){desktop_io_t *j=context;return !atomic_load(&j->cancel) && fwrite(data,1,n,j->file)==n;}
static void *worker(void *context){desktop_io_t *j=context;
    if(j->kind==DESKTOP_LIST_PROFILES){DIR *directory=opendir(j->path);if(directory){struct dirent *entry;while(j->saved_count<64 && !atomic_load(&j->cancel) && (entry=readdir(directory))){size_t n=strlen(entry->d_name);if(n<=11 || strcmp(entry->d_name+n-11,".lanprofile"))continue;desktop_saved_profile_t *s=&j->saved[j->saved_count];int length=snprintf(s->path,sizeof(s->path),"%s/%s",j->path,entry->d_name);if(length<0 || (size_t)length>=sizeof(s->path))continue;snprintf(s->name,sizeof(s->name),"%s",entry->d_name);char caption[4096];if(snprintf(caption,sizeof(caption),"%s.name",s->path)>0 && strlen(s->path)+6<sizeof(caption)){FILE *file=fopen(caption,"rb");if(file){size_t used=fread(s->name,1,sizeof(s->name)-1,file);s->name[used]=0;fclose(file);}}++j->saved_count;}closedir(directory);}j->success=true;
    }else if(j->kind==DESKTOP_OPEN_PROFILE){char *buffer=malloc(65537);FILE *file=fopen(j->path,"rb");if(buffer && file){size_t n=fread(buffer,1,65537,file);j->success=!ferror(file) && n<=65536 && !atomic_load(&j->cancel) && profile_parse(buffer,n,j->profile,NULL,0);}if(file)fclose(file);free(buffer);
    }else{char temporary[4096];int length=snprintf(temporary,sizeof(temporary),"%s.XXXXXX",j->path);if(length>0 && (size_t)length<sizeof(temporary)){int fd=mkstemp(temporary);if(fd>=0){j->file=fdopen(fd,"wb");if(j->file){bool ok=j->kind==DESKTOP_EXPORT ? report_write(j->format,j->hosts,j->count,&j->vendors,&j->options,writer,j):profile_write(j->profile,writer,j);if(fflush(j->file) || fsync(fd))ok=false;if(fclose(j->file))ok=false;j->file=NULL;if(ok && !atomic_load(&j->cancel) && !rename(temporary,j->path))j->success=true;}else close(fd);if(!j->success)unlink(temporary);}}}
    if(j->kind==DESKTOP_SAVE_PROFILE && j->success && j->managed[0] && !atomic_load(&j->cancel)){
        if(!mkdir(j->managed,0700) || access(j->managed,W_OK)==0){uint64_t hash=14695981039346656037ull;for(const unsigned char *s=(const unsigned char *)j->profile->name;*s;++s){hash^=*s;hash*=1099511628211ull;}hash^=0;hash*=1099511628211ull;for(const unsigned char *s=(const unsigned char *)j->profile->interface_id;*s;++s){hash^=*s;hash*=1099511628211ull;}char path[4096],temporary[4096];int length=snprintf(path,sizeof(path),"%s/%016llx.lanprofile",j->managed,(unsigned long long)hash);
            if(length>0 && (size_t)length+8<sizeof(path)){snprintf(temporary,sizeof(temporary),"%s.XXXXXX",path);int fd=mkstemp(temporary);if(fd>=0){j->file=fdopen(fd,"wb");bool ok=false;if(j->file){ok=profile_write(j->profile,writer,j);if(fflush(j->file) || fsync(fd))ok=false;if(fclose(j->file))ok=false;j->file=NULL;}else close(fd);if(!ok || atomic_load(&j->cancel) || rename(temporary,path))unlink(temporary);else{char namepath[4096];snprintf(namepath,sizeof(namepath),"%s.name",path);snprintf(temporary,sizeof(temporary),"%s.XXXXXX",namepath);int namefd=mkstemp(temporary);if(namefd>=0){FILE *name=fdopen(namefd,"wb");bool written=false;if(name){written=fputs(j->profile->name,name)>=0;if(fflush(name) || fsync(namefd))written=false;if(fclose(name))written=false;}else close(namefd);if(!written || rename(temporary,namepath))unlink(temporary);}}}}
        }
    }
    atomic_store(&j->done,true);return NULL;
}
desktop_io_t *desktop_io_start(desktop_io_kind_t kind,const char *path,const profile_t *profile,const lan_host_t *hosts,size_t count,const vendor_table_t *vendors,report_format_t format,bool russian,const char *managed_directory){
    if(!path || strlen(path)+8>=4096 || count>65536)return NULL;desktop_io_t *j=calloc(1,sizeof(*j));if(!j)return NULL;atomic_init(&j->cancel,false);atomic_init(&j->done,false);j->kind=kind;lan_text_copy(j->path,sizeof(j->path),path);j->format=format;j->count=count;j->vendors=*vendors;j->profile=malloc(sizeof(*j->profile));if(!j->profile){free(j);return NULL;}*j->profile=*profile;
    if(managed_directory && strlen(managed_directory)+64<sizeof(j->managed))lan_text_copy(j->managed,sizeof(j->managed),managed_directory);
    if(kind==DESKTOP_EXPORT && count){j->hosts=malloc(count*sizeof(*j->hosts));if(!j->hosts){free(j->profile);free(j);return NULL;}memcpy(j->hosts,hosts,count*sizeof(*hosts));}
    j->options.russian=russian;snprintf(j->options.profile_name,sizeof(j->options.profile_name),"%s",profile->name);snprintf(j->options.copyright,sizeof(j->options.copyright),"© nr084, " APP_BUILD_YEAR);
    if(pthread_create(&j->thread,NULL,worker,j)){free(j->hosts);free(j->profile);free(j);return NULL;}return j;
}
void desktop_io_cancel(desktop_io_t *j){if(j)atomic_store(&j->cancel,true);}
bool desktop_io_finished(const desktop_io_t *j){return atomic_load(&j->done);}
desktop_io_kind_t desktop_io_kind(const desktop_io_t *j){return j->kind;}
size_t desktop_io_profiles(const desktop_io_t *j,desktop_saved_profile_t out[64]){if(!desktop_io_finished(j))return 0;memcpy(out,j->saved,j->saved_count*sizeof(*out));return j->saved_count;}
bool desktop_io_result(desktop_io_t *j,profile_t *opened,bool *cancelled){if(!desktop_io_finished(j))return false;if(!j->joined){pthread_join(j->thread,NULL);j->joined=true;}if(cancelled)*cancelled=atomic_load(&j->cancel);if(opened && j->success && j->kind==DESKTOP_OPEN_PROFILE)*opened=*j->profile;return j->success;}
void desktop_io_free(desktop_io_t *j){if(!j || !j->joined)return;free(j->profile);free(j->hosts);free(j);}
