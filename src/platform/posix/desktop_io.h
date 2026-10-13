#ifndef DESKTOP_IO_H
#define DESKTOP_IO_H
#include "profile.h"
#include "report.h"
typedef enum {DESKTOP_EXPORT,DESKTOP_SAVE_PROFILE,DESKTOP_OPEN_PROFILE,DESKTOP_LIST_PROFILES} desktop_io_kind_t;
typedef struct {char name[256],path[4096];} desktop_saved_profile_t;
typedef struct desktop_io desktop_io_t;
desktop_io_t *desktop_io_start(desktop_io_kind_t kind,const char *path,const profile_t *profile,const lan_host_t *hosts,size_t count,const vendor_table_t *vendors,report_format_t format,bool russian,const char *managed_directory);
void desktop_io_cancel(desktop_io_t *job);
bool desktop_io_finished(const desktop_io_t *job);
bool desktop_io_result(desktop_io_t *job,profile_t *opened,bool *cancelled);
desktop_io_kind_t desktop_io_kind(const desktop_io_t *job);
size_t desktop_io_profiles(const desktop_io_t *job,desktop_saved_profile_t out[64]);
void desktop_io_free(desktop_io_t *job);
#endif
