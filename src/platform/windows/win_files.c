#include "win_files.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct { HANDLE file; char buffer[65536]; size_t used; volatile LONG *cancel; DWORD error; wchar_t temporary[MAX_PATH]; } file_writer_t;
static volatile LONG file_sequence;
static bool flush(file_writer_t *w) {
    if (w->cancel && InterlockedCompareExchange(w->cancel,0,0)) { w->error = ERROR_CANCELLED; return false; }
    if (!w->used) return true; DWORD written;
    if (!WriteFile(w->file,w->buffer,(DWORD)w->used,&written,NULL) || written != w->used) { w->error = GetLastError(); if (!w->error) w->error = ERROR_WRITE_FAULT; return false; }
    w->used = 0; return true;
}
static bool write_bytes(void *context, const char *s, size_t n) {
    file_writer_t *w = context;
    while (n) { size_t available = sizeof(w->buffer)-w->used; size_t copy = n < available ? n : available; memcpy(w->buffer+w->used,s,copy); w->used += copy; s += copy; n -= copy; if (w->used == sizeof(w->buffer) && !flush(w)) return false; }
    return true;
}
static bool begin_write(file_writer_t *w, const wchar_t *path, volatile LONG *cancel) {
    memset(w,0,sizeof(*w)); w->file = INVALID_HANDLE_VALUE; w->cancel = cancel;
    if (wcslen(path) > MAX_PATH-48) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return false; }
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        swprintf_s(w->temporary,MAX_PATH,L"%ls.tmp-%lu-%ld",path,GetCurrentProcessId(),InterlockedIncrement(&file_sequence));
        w->file = CreateFileW(w->temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if (w->file != INVALID_HANDLE_VALUE) return true;
        if (GetLastError() != ERROR_FILE_EXISTS) return false;
    }
    return false;
}
static bool finish_write(file_writer_t *w, const wchar_t *path, bool success) {
    if (success) success = flush(w);
    if (success && !FlushFileBuffers(w->file)) { w->error = GetLastError(); success = false; }
    CloseHandle(w->file); w->file = INVALID_HANDLE_VALUE;
    if (success && w->cancel && InterlockedCompareExchange(w->cancel,0,0)) { w->error = ERROR_CANCELLED; success = false; }
    if (success && !MoveFileExW(w->temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { w->error = GetLastError(); success = false; }
    if (!success) { DeleteFileW(w->temporary); if (!w->error) w->error = ERROR_INVALID_DATA; SetLastError(w->error); }
    return success;
}
bool win_profile_load(const wchar_t *path, profile_t *profile, char *error, size_t capacity) {
    HANDLE file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size; bool ok = GetFileSizeEx(file,&size) && size.QuadPart > 0 && size.QuadPart <= 65536;
    DWORD failure = ok ? NO_ERROR : ERROR_INVALID_DATA;
    char *bytes = ok ? malloc((size_t)size.QuadPart) : NULL;
    if (ok && !bytes) { failure = ERROR_NOT_ENOUGH_MEMORY; ok = false; }
    DWORD read = 0;
    if (ok && (!ReadFile(file,bytes,(DWORD)size.QuadPart,&read,NULL) || read != size.QuadPart)) { failure = GetLastError(); if (!failure) failure = ERROR_HANDLE_EOF; ok = false; }
    CloseHandle(file);
    if (ok && !profile_parse(bytes,read,profile,error,capacity)) { failure = ERROR_INVALID_DATA; ok = false; }
    free(bytes); if (!ok) SetLastError(failure); return ok;
}
bool win_profile_save(const wchar_t *path, const profile_t *profile) {
    if (!profile_validate(profile,NULL,0)) { SetLastError(ERROR_INVALID_DATA); return false; }
    file_writer_t writer; if (!begin_write(&writer,path,NULL)) return false;
    return finish_write(&writer,path,profile_write(profile,write_bytes,&writer));
}
bool win_profile_directory(wchar_t *path, size_t capacity, bool create) {
    wchar_t local[MAX_PATH], portable[MAX_PATH]; DWORD n = GetModuleFileNameW(NULL,portable,MAX_PATH);
    if (!n || n >= MAX_PATH-20) return false; wchar_t *slash = wcsrchr(portable,L'\\'); if (!slash) return false;
    swprintf_s(slash,MAX_PATH-(size_t)(slash-portable),L"\\profiles");
    n = GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH); bool fallback = n && n < MAX_PATH-32;
    if (fallback) wcscat_s(local,MAX_PATH,L"\\Look@LAN-ng\\profiles");
    const wchar_t *chosen = portable;
    if (create) {
        if (!CreateDirectoryW(portable,NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            if (!fallback) return false; wchar_t parent[MAX_PATH]; wcscpy_s(parent,MAX_PATH,local); wchar_t *last = wcsrchr(parent,L'\\'); *last = 0; CreateDirectoryW(parent,NULL);
            if (!CreateDirectoryW(local,NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return false; chosen = local;
        }
    } else if (GetFileAttributesW(portable) == INVALID_FILE_ATTRIBUTES && fallback && GetFileAttributesW(local) != INVALID_FILE_ATTRIBUTES) chosen = local;
    return wcscpy_s(path,capacity,chosen) == 0;
}
size_t win_saved_profiles(saved_profile_t *out, size_t capacity) {
    if (!out || !capacity) return 0;
    wchar_t directory[MAX_PATH], pattern[MAX_PATH]; if (!win_profile_directory(directory,MAX_PATH,false) || wcslen(directory) > MAX_PATH-20) return 0;
    swprintf_s(pattern,MAX_PATH,L"%ls\\*.lanprofile",directory); WIN32_FIND_DATAW data; HANDLE search = FindFirstFileW(pattern,&data);
    if (search == INVALID_HANDLE_VALUE) return 0;
    profile_t *profile = malloc(sizeof(*profile)); size_t count = 0;
    if (profile) do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        wchar_t path[MAX_PATH]; if (wcslen(directory)+wcslen(data.cFileName)+2 > MAX_PATH) continue;
        swprintf_s(path,MAX_PATH,L"%ls\\%ls",directory,data.cFileName);
        if (!win_profile_load(path,profile,NULL,0)) continue;
        wcscpy_s(out[count].path,MAX_PATH,path); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,profile->name,-1,out[count].name,256); ++count;
    } while (count < capacity && FindNextFileW(search,&data));
    free(profile); FindClose(search); return count;
}
struct win_export_job {
    HANDLE thread;
    volatile LONG cancel;
    DWORD error;
    wchar_t path[MAX_PATH];
    report_format_t format;
    host_info_t *hosts;
    size_t count;
    vendor_table_t vendors;
    report_options_t options;
};
static DWORD WINAPI export_worker(void *parameter) {
    win_export_job_t *job = parameter; file_writer_t writer;
    if (!begin_write(&writer,job->path,&job->cancel)) job->error = GetLastError();
    else if (!finish_write(&writer,job->path,report_write(job->format,job->hosts,job->count,&job->vendors,&job->options,write_bytes,&writer))) job->error = GetLastError();
    free(job->hosts); job->hosts = NULL; return 0;
}
win_export_job_t *win_export_start(const wchar_t *path, report_format_t format, const host_info_t *hosts, size_t count, const vendor_table_t *vendors, const report_options_t *options) {
    if (!path || wcslen(path) > MAX_PATH-48 || count > SIZE_MAX/sizeof(*hosts) || !vendors || !options || (count && !hosts)) return NULL;
    win_export_job_t *job = calloc(1,sizeof(*job)); if (!job) return NULL;
    if (count) { job->hosts = malloc(count*sizeof(*hosts)); if (!job->hosts) { free(job); return NULL; } memcpy(job->hosts,hosts,count*sizeof(*hosts)); }
    job->count = count; job->format = format; job->vendors = *vendors; job->options = *options; wcscpy_s(job->path,MAX_PATH,path);
    job->thread = CreateThread(NULL,0,export_worker,job,0,NULL);
    if (!job->thread) { free(job->hosts); free(job); return NULL; }
    return job;
}
bool win_export_done(win_export_job_t *job, DWORD *error) { if (!job || WaitForSingleObject(job->thread,0) != WAIT_OBJECT_0) return false; if (error) *error = job->error; return true; }
void win_export_cancel(win_export_job_t *job) { if (job) { InterlockedExchange(&job->cancel,1); CancelSynchronousIo(job->thread); } }
void win_export_release(win_export_job_t *job) { if (job && WaitForSingleObject(job->thread,0) == WAIT_OBJECT_0) { CloseHandle(job->thread); free(job); } }
