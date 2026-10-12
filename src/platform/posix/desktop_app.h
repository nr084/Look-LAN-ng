#ifndef DESKTOP_APP_H
#define DESKTOP_APP_H
#include "posix_net.h"
#include "monitor.h"
#include "vendor.h"
#include "report.h"
#define DESKTOP_HOSTS 65536
typedef struct {
    posix_backend_t backend; posix_interface_t interfaces[POSIX_INTERFACES]; size_t interface_count,selected;
    lan_host_t *hosts;size_t host_count;monitor_event_t *log;monitor_t monitor;profile_t profile;
    vendor_table_t vendors;bool russian,scanning,closing,closed,failed;unsigned language;uint32_t completed,total;
    char settings[4096],error[256];
} desktop_app_t;
bool desktop_init(desktop_app_t *app,const char *system_language);
void desktop_language(desktop_app_t *app,unsigned language,const char *system_language);
bool desktop_refresh(desktop_app_t *app);
bool desktop_select(desktop_app_t *app,size_t index);
bool desktop_start(desktop_app_t *app,const char *range);
bool desktop_poll(desktop_app_t *app);
bool desktop_close(desktop_app_t *app);
const char *desktop_tr(const desktop_app_t *app,const char *en,const char *ru);
void desktop_cell(const desktop_app_t *app,size_t host,unsigned column,char *out,size_t capacity);
bool desktop_export(const desktop_app_t *app,const char *path,report_format_t format);
bool desktop_profile_open(desktop_app_t *app,const char *path);
bool desktop_profile_save(const desktop_app_t *app,const char *path);
int desktop_smoke(const char *system_language);
#endif
