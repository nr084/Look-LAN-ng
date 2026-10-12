#include "win_tools.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <time.h>
#include <stdlib.h>
struct win_tools_job {
    HANDLE thread; CRITICAL_SECTION lock; volatile LONG cancel;
    win_ping_pool_t *pool;
    uint32_t source, target, interface_index, timeout;
    bool trace; size_t count, read;
    ping_sample_t samples[30];
};
static bool stopped(void *context) { return InterlockedCompareExchange(&((win_tools_job_t *)context)->cancel,0,0) != 0; }
static DWORD WINAPI run_tool(void *context) {
    win_tools_job_t *job = context;
    for (unsigned i = 0; i < (job->trace ? 30u : 4u) && !stopped(job); ++i) {
        host_info_t host = {0}; win_echo_result_t echo;
        scan_result_t result = win_ping_probe_ttl(job->pool,job->source,job->target,job->timeout,job->trace ? (uint8_t)(i+1) : 0,&host,&echo,stopped,job);
        if (result == SCAN_CANCELLED || stopped(job)) break;
        ping_sample_t sample = {0}; sample.ip = job->target; sample.interface_index = job->interface_index; sample.timestamp = (uint64_t)time(NULL);
        sample.latency_ms = echo.latency_ms; sample.status = echo.status; sample.received = echo.received; sample.reply_ip = echo.address; sample.hop = job->trace ? (uint8_t)(i+1) : 0;
        EnterCriticalSection(&job->lock); job->samples[job->count++] = sample; LeaveCriticalSection(&job->lock);
        if (job->trace && echo.received && echo.status == IP_SUCCESS) break;
        if (job->trace && echo.received && echo.status != IP_TTL_EXPIRED_TRANSIT && echo.status != IP_TTL_EXPIRED_REASSEM) break;
        if (!job->trace && i < 3) for (unsigned wait = 0; wait < 40 && !stopped(job); ++wait) Sleep(25);
    }
    return 0;
}
win_tools_job_t *win_tools_start(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t index, uint32_t timeout, bool trace) {
    if (!pool || !pool->initialized || !source || !target || !timeout || timeout > 5000) return NULL;
    win_tools_job_t *job = calloc(1,sizeof(*job)); if (!job) return NULL;
    job->pool = pool; job->source = source; job->target = target; job->interface_index = index; job->timeout = timeout; job->trace = trace;
    InitializeCriticalSection(&job->lock); job->thread = CreateThread(NULL,0,run_tool,job,0,NULL);
    if (!job->thread) { DeleteCriticalSection(&job->lock); free(job); return NULL; } return job;
}
void win_tools_cancel(win_tools_job_t *job) { if (job) InterlockedExchange(&job->cancel,1); }
bool win_tools_pop(win_tools_job_t *job, ping_sample_t *sample) {
    if (!job) return false; EnterCriticalSection(&job->lock); bool found = job->read < job->count;
    if (found) *sample = job->samples[job->read++]; LeaveCriticalSection(&job->lock); return found;
}
bool win_tools_finished(win_tools_job_t *job) { return job && WaitForSingleObject(job->thread,0) == WAIT_OBJECT_0; }
void win_tools_free(win_tools_job_t *job) { if (!job || !win_tools_finished(job)) return; CloseHandle(job->thread); DeleteCriticalSection(&job->lock); free(job); }
