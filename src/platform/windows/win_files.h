#ifndef WIN_FILES_H
#define WIN_FILES_H
#include <windows.h>
#include "profile.h"
#include "report.h"
typedef struct { wchar_t path[MAX_PATH], name[256]; } saved_profile_t;
bool win_profile_directory(wchar_t *path, size_t capacity, bool create);
size_t win_saved_profiles(saved_profile_t *profiles, size_t capacity);
bool win_profile_load(const wchar_t *path, profile_t *profile, char *error, size_t error_capacity);
bool win_profile_save(const wchar_t *path, const profile_t *profile);
typedef struct win_export_job win_export_job_t;
/* Job owns a snapshot; vendor resource storage must outlive the job. */
win_export_job_t *win_export_start(const wchar_t *path, report_format_t format, const host_info_t *hosts, size_t count, const vendor_table_t *vendors, const report_options_t *options);
bool win_export_done(win_export_job_t *job, DWORD *error);
void win_export_cancel(win_export_job_t *job);
void win_export_release(win_export_job_t *job);
#endif
