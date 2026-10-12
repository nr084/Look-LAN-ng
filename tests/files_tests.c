#include "win_files.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static profile_t profile, loaded;
static DWORD wait_export(win_export_job_t *job) {
    if (!job) return ERROR_NOT_ENOUGH_MEMORY;
    ULONGLONG deadline = GetTickCount64()+10000; DWORD error = ERROR_TIMEOUT;
    while (!win_export_done(job,&error) && GetTickCount64() < deadline) Sleep(1);
    if (!win_export_done(job,&error)) { win_export_cancel(job); return ERROR_TIMEOUT; }
    win_export_release(job); return error;
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 1;
    wchar_t profile_path[MAX_PATH], report_path[MAX_PATH];
    if (wcslen(argv[1]) > MAX_PATH-50) return 2;
    swprintf_s(profile_path,MAX_PATH,L"%ls\\profile-test-%lu.lanprofile",argv[1],GetCurrentProcessId());
    swprintf_s(report_path,MAX_PATH,L"%ls\\report-test-%lu.html",argv[1],GetCurrentProcessId());
    profile_defaults(&profile); profile.range_count = 1; profile.ranges[0] = (ipv4_range_t){0xc0a80101,0xc0a801fe}; snprintf(profile.name,sizeof(profile.name),"Офис"); snprintf(profile.notes,sizeof(profile.notes),"first\nsecond");
    int result = 0;
    if (!win_profile_save(profile_path,&profile) || !win_profile_load(profile_path,&loaded,NULL,0) || strcmp(profile.name,loaded.name) || strcmp(profile.notes,loaded.notes)) { result = 3; goto cleanup; }
    vendor_table_t vendors = {0}; report_options_t options = {0}; memcpy(options.profile_name,"snapshot",9); memcpy(options.copyright,"nr084",6);
    lan_host_t host = {0}; host.address = ipv4_address(0xc0a80101); host.online = true; host.first_seen = 100; memcpy(host.hostname,"before",7);
    win_export_job_t *job = win_export_start(report_path,REPORT_HTML,&host,1,&vendors,&options); memcpy(host.hostname,"after!",7);
    if (wait_export(job) != NO_ERROR) { result = 4; goto cleanup; }
    HANDLE file = CreateFileW(report_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); char bytes[8192] = {0}; DWORD n = 0;
    if (file == INVALID_HANDLE_VALUE || !ReadFile(file,bytes,sizeof(bytes)-1,&n,NULL)) { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); result = 5; goto cleanup; } CloseHandle(file);
    if (!strstr(bytes,"before") || strstr(bytes,"after!")) { result = 6; goto cleanup; }
    /* Cancellation must not replace the previously valid destination. A large
       snapshot keeps the worker busy until the explicit cancellation. */
    lan_host_t *many = calloc(4096,sizeof(*many)); if (!many) { result = 7; goto cleanup; }
    for (unsigned i = 0; i < 4096; ++i) many[i] = host;
    job = win_export_start(report_path,REPORT_HTML,many,4096,&vendors,&options); free(many); win_export_cancel(job);
    DWORD cancelled = wait_export(job); if (cancelled != ERROR_CANCELLED && cancelled != ERROR_OPERATION_ABORTED) { result = 8; goto cleanup; }
    file = CreateFileW(report_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); memset(bytes,0,sizeof(bytes));
    if (file == INVALID_HANDLE_VALUE || !ReadFile(file,bytes,sizeof(bytes)-1,&n,NULL)) { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); result = 9; goto cleanup; } CloseHandle(file);
    if (!strstr(bytes,"before") || strstr(bytes,"after!")) result = 10;
cleanup:
    DeleteFileW(profile_path); DeleteFileW(report_path);
    if (!result) puts("Files tests passed: Unicode paths/profile round-trip, async export snapshot, cancellation retains destination, temporary outputs cleaned.");
    else fprintf(stderr,"Files test failure: %d, Windows error %lu\n",result,GetLastError());
    return result;
}
