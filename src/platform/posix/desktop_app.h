#ifndef DESKTOP_APP_H
#define DESKTOP_APP_H
#include "posix_net.h"
#include "monitor.h"
#include "vendor.h"
#include "report.h"
#include "details.h"
#include "posix_tools.h"
#include "desktop_io.h"
#define DESKTOP_HOSTS 65536
typedef struct {
    posix_backend_t backend; posix_interface_t interfaces[POSIX_INTERFACES]; size_t interface_count,selected;
    lan_host_t *hosts;size_t host_count;monitor_event_t *log;monitor_t monitor;profile_t profile;
    vendor_table_t vendors;details_store_t *details;bool russian,scanning,monitoring,closing,closed,failed;unsigned language;uint32_t completed,total;
    uint64_t next_round;
    posix_tools_t *tools;bool tracing;ping_sample_t trace[30];size_t trace_count;uint32_t trace_ip,trace_interface;
    desktop_io_t *io;profile_t *pending_profile;bool profile_changed;
    bool resolving,resolve_cancelled;uint32_t resolved_ip;
    char profiles_directory[4096];desktop_saved_profile_t saved_profiles[64];size_t saved_count;bool saved_changed;
    bool names_pending,arp_only[DESKTOP_HOSTS],cached_round[DESKTOP_HOSTS];uint8_t round_proof[DESKTOP_HOSTS];
    bool row_dirty[DESKTOP_HOSTS];
    char settings[4096],error[256];
} desktop_app_t;
bool desktop_init(desktop_app_t *app,const char *system_language);
void desktop_language(desktop_app_t *app,unsigned language,const char *system_language);
bool desktop_refresh(desktop_app_t *app);
bool desktop_select(desktop_app_t *app,size_t index);
bool desktop_start(desktop_app_t *app,const char *range);
bool desktop_single(desktop_app_t *app,uint32_t ip);
bool desktop_single_target(desktop_app_t *app,const char *target);
uint64_t desktop_scope_size(const desktop_app_t *app,const char *range);
bool desktop_monitor(desktop_app_t *app,const char *range);
void desktop_stop(desktop_app_t *app);
bool desktop_profile_configure(desktop_app_t *app,const char *name,const char *ranges,const char *ports,unsigned icmp,unsigned tcp,unsigned concurrency,unsigned interval,const char *notes);
void desktop_statistics_text(const desktop_app_t *app,char *out,size_t capacity);
void desktop_log_cell(const desktop_app_t *app,size_t row,unsigned column,char *out,size_t capacity);
void desktop_history(const desktop_app_t *app,char *out,size_t capacity);
bool desktop_diagnostic(desktop_app_t *app,uint32_t ip,bool trace);
void desktop_detail(const desktop_app_t *app,size_t index,char *out,size_t capacity);
void desktop_detail_part(const desktop_app_t *app,size_t index,unsigned part,char *out,size_t capacity);
int desktop_compare(const desktop_app_t *app,size_t first,size_t second,unsigned column);
void desktop_network_text(const desktop_app_t *app,char *out,size_t capacity);
bool desktop_reload_profiles(desktop_app_t *app);
bool desktop_poll(desktop_app_t *app);
bool desktop_close(desktop_app_t *app);
const char *desktop_tr(const desktop_app_t *app,const char *en,const char *ru);
void desktop_cell(const desktop_app_t *app,size_t host,unsigned column,char *out,size_t capacity);
bool desktop_export(desktop_app_t *app,const char *path,report_format_t format);
bool desktop_profile_rebind(desktop_app_t *app,size_t interface_index);
bool desktop_profile_open(desktop_app_t *app,const char *path);
bool desktop_profile_save(desktop_app_t *app,const char *path);
int desktop_smoke(const char *system_language);
#endif
