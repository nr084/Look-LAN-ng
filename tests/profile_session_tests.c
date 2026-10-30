#include "desktop_app.h"
#include "statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s (%s)\n",__LINE__,#x,a?a->error:"");return 1;}}while(0)
static void pause_tick(void){struct timespec t={0,5000000};nanosleep(&t,NULL);}
static bool idle(desktop_app_t *a){for(unsigned i=0;i<4000;++i){desktop_poll(a);if(!a->io && !a->history_job && !a->history_pending && !a->scanning)return true;pause_tick();}return false;}
static bool close_app(desktop_app_t *a){for(unsigned i=0;i<4000;++i){if(desktop_close(a))return true;pause_tick();}return false;}
static void paths(desktop_app_t *a,const char *base){snprintf(a->settings,sizeof(a->settings),"%s/Look@LAN-ng.ini",base);snprintf(a->profiles_directory,sizeof(a->profiles_directory),"%s/profiles",base);}
int main(void){
    char base[4096];if(!getcwd(base,sizeof(base)) || strlen(base)+40>=sizeof(base))return 1;strcat(base,"/looklan-profile-session-XXXXXX");if(!mkdtemp(base))return 1;
    desktop_app_t *a=calloc(1,sizeof(*a));CHECK(a && desktop_init(a,"en"));if(a->selected>=a->interface_count){close_app(a);free(a);rmdir(base);return 77;}paths(a,base);
    desktop_language(a,2,"en");CHECK(access(a->settings,F_OK)!=0 && access(a->profiles_directory,F_OK)!=0);
    uint32_t own=a->interfaces[a->selected].ip;CHECK(desktop_single(a,own) && idle(a));CHECK(a->host_count>0 && !a->persistent && access(a->settings,F_OK)!=0);
    CHECK(desktop_profile_save(a,"Office.lanprofile") && idle(a));CHECK(a->persistent && access(a->settings,F_OK)==0);
    char profile[4096],history[4096],settings[4096],directory[4096];snprintf(profile,sizeof(profile),"%s",a->profile_path);snprintf(history,sizeof(history),"%s",a->history_path);snprintf(settings,sizeof(settings),"%s",a->settings);snprintf(directory,sizeof(directory),"%s",a->profiles_directory);
    CHECK(history_store_exists(history));uint64_t first=a->hosts[0].first_seen,sequence=a->monitor.sequence;CHECK(first && sequence);
    CHECK(close_app(a));free(a);a=calloc(1,sizeof(*a));CHECK(a && desktop_init(a,"en"));paths(a,base);CHECK(desktop_reload_profiles(a) && idle(a));
    CHECK(a->persistent && a->saved_count==1 && !strcmp(a->profile_path,profile) && a->host_count>0 && a->hosts[0].historical && a->hosts[0].first_seen==first && a->monitor.sequence>=sequence);
    CHECK(statistics_compute(a->hosts,a->host_count,0).online==0);
    CHECK(desktop_single(a,own) && idle(a));CHECK(!a->hosts[0].historical && a->hosts[0].first_seen==first && a->monitor.sequence>sequence);
    CHECK(desktop_refresh(a));CHECK(!strcmp(a->profile_path,profile) && a->host_count>0);
    CHECK(close_app(a));free(a);unlink(history);unlink(profile);unlink(settings);rmdir(directory);rmdir(base);
    puts("PASS: temporary scan has no INI/directories; save activates persistence; singleton autoload, history and monitoring continuity");return 0;
}
