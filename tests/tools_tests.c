#include "win_tools.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Tools check failed at %d: %s\n",__LINE__,#x); return 1; } } while (0)
static int mode; static volatile LONG calls, bad_binding;
static scan_result_t fake_probe(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t timeout, uint8_t ttl, lan_host_t *host, win_echo_result_t *echo, bool (*cancel)(void *), void *context) {
    (void)pool; (void)timeout; (void)host;
    if (source != 1 || target != 2) InterlockedIncrement(&bad_binding);
    InterlockedIncrement(&calls); memset(echo,0,sizeof(*echo));
    if (mode == 2) { while (!cancel(context)) Sleep(1); return SCAN_CANCELLED; }
    if (mode == 3) { echo->status = IP_REQ_TIMED_OUT; return SCAN_TIMEOUT; }
    echo->received = true; echo->latency_ms = 7; echo->address = ttl == 1 ? 0xc0a80101 : 2; echo->status = ttl == 1 ? IP_TTL_EXPIRED_TRANSIT : IP_SUCCESS;
    return ttl == 1 ? SCAN_UNREACHABLE : SCAN_OK;
}
#define win_ping_probe_ttl fake_probe
#include "../src/platform/windows/win_tools.c"
static bool wait_job(win_tools_job_t *job) { ULONGLONG deadline = GetTickCount64()+6000; while (!win_tools_finished(job)) { if (GetTickCount64() > deadline) return false; Sleep(1); } return true; }
int main(void) {
    win_ping_pool_t pool = {0}; pool.initialized = true;
    CHECK(!win_tools_start(NULL,1,2,9,100,true) && !win_tools_start(&pool,1,2,9,0,true));
    mode = 1; win_tools_job_t *job = win_tools_start(&pool,1,2,9,100,true); CHECK(job && wait_job(job));
    ping_sample_t sample;
    CHECK(win_tools_pop(job,&sample) && sample.hop == 1 && sample.reply_ip == 0xc0a80101 && sample.status == IP_TTL_EXPIRED_TRANSIT && sample.interface_index == 9);
    CHECK(win_tools_pop(job,&sample) && sample.hop == 2 && sample.status == IP_SUCCESS && sample.reply_ip == 2 && sample.ip == 2 && sample.latency_ms == 7 && sample.timestamp);
    CHECK(!win_tools_pop(job,&sample)); win_tools_free(job);
    mode = 3; job = win_tools_start(&pool,1,2,9,100,true); CHECK(job && wait_job(job)); unsigned hops = 0;
    while (win_tools_pop(job,&sample)) { CHECK(!sample.received && sample.hop == ++hops && sample.status == IP_REQ_TIMED_OUT); } CHECK(hops == 30); win_tools_free(job);
    mode = 0; job = win_tools_start(&pool,1,2,9,100,false); CHECK(job && wait_job(job)); unsigned count = 0;
    while (win_tools_pop(job,&sample)) { CHECK(!sample.hop && !sample.status && sample.received); ++count; } CHECK(count == 4); win_tools_free(job);
    mode = 2; calls = 0; job = win_tools_start(&pool,1,2,9,5000,true); CHECK(job);
    ULONGLONG deadline = GetTickCount64()+1000; while (!calls) { CHECK(GetTickCount64() < deadline); Sleep(1); }
    ULONGLONG cancel_started = GetTickCount64(); win_tools_cancel(job); CHECK(wait_job(job) && GetTickCount64()-cancel_started < 500);
    CHECK(!win_tools_pop(job,&sample)); win_tools_free(job); CHECK(!bad_binding);
    puts("Tools passed: source binding, TTL hop progression/destination stop, four ping samples, buffered results and pending cancellation."); return 0;
}
