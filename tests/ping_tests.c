/* Real implementation with deterministic ICMP doubles: cancelled buffers must
   remain valid until the API event signals. No network traffic. */
#include "win_ping.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Ping check failed, line %d: %s\n",__LINE__,#x); return 1; } } while (0)
enum { SYNC_REPLY, ASYNC_REPLY, PENDING_REPLY, FAILED_REPLY };
typedef struct { HANDLE icmp, event; void *reply, *payload; PIP_OPTION_INFORMATION options; bool pending; } fake_query_t;
static fake_query_t queries[PING_CAPACITY];
static int mode, checks;
static DWORD reply_status;
static uint8_t last_ttl;
static volatile LONG premature_close, created, closed;
static HANDLE WINAPI fake_create(void) {
    HANDLE handle = CreateEventW(NULL,TRUE,FALSE,NULL);
    if (handle) { for (size_t i = 0; i < PING_CAPACITY; ++i) if (!queries[i].icmp) { queries[i].icmp = handle; InterlockedIncrement(&created); return handle; } CloseHandle(handle); }
    return INVALID_HANDLE_VALUE;
}
static BOOL WINAPI fake_close(HANDLE handle) {
    for (size_t i = 0; i < PING_CAPACITY; ++i) if (queries[i].icmp == handle) {
        if (queries[i].pending && WaitForSingleObject(queries[i].event,0) != WAIT_OBJECT_0) InterlockedIncrement(&premature_close);
        queries[i] = (fake_query_t){0}; InterlockedIncrement(&closed); return CloseHandle(handle);
    }
    return FALSE;
}
static DWORD WINAPI fake_send(HANDLE handle, HANDLE event, void *callback, void *context, IPAddr source, IPAddr target, void *payload, WORD payload_size, PIP_OPTION_INFORMATION options, void *reply, DWORD reply_size, DWORD timeout) {
    (void)callback; (void)context; (void)source; (void)target; (void)payload_size; (void)options; (void)reply_size; (void)timeout;
    if (mode == FAILED_REPLY) { SetLastError(IP_DEST_HOST_UNREACHABLE); return 0; }
    last_ttl = options ? options->Ttl : 0;
    ICMP_ECHO_REPLY *answer = reply; answer->Status = reply_status; answer->Address = htonl(0xc0a80101); answer->RoundTripTime = 123; answer->Options.Ttl = 64;
    if (mode == SYNC_REPLY) return 1;
    for (size_t i = 0; i < PING_CAPACITY; ++i) if (queries[i].icmp == handle) { queries[i].event = event; queries[i].reply = reply; queries[i].payload = payload; queries[i].options = options; queries[i].pending = true; break; }
    if (mode == ASYNC_REPLY) SetEvent(event);
    SetLastError(ERROR_IO_PENDING); return 0;
}
static DWORD WINAPI fake_parse(void *reply, DWORD size) { (void)reply; (void)size; return 1; }
#define IcmpCreateFile fake_create
#define IcmpCloseHandle fake_close
#define IcmpSendEcho2Ex fake_send
#define IcmpParseReplies fake_parse
#include "../src/platform/windows/win_ping.c"
static bool never_cancel(void *context) { (void)context; return false; }
static bool cancel_soon(void *context) { (void)context; return ++checks >= 2; }
static size_t pending_count(win_ping_pool_t *pool) { EnterCriticalSection(&pool->lock); size_t count = pool->count; LeaveCriticalSection(&pool->lock); return count; }
static bool complete_pending(win_ping_pool_t *pool) {
    /* Reaper also takes this lock before freeing/query close. */
    EnterCriticalSection(&pool->lock);
    for (size_t i = 0; i < PING_CAPACITY; ++i) if (queries[i].pending) {
        if (memcmp(queries[i].payload,"Look@LAN-ng",12)) { LeaveCriticalSection(&pool->lock); return false; }
        if (queries[i].options && queries[i].options->Ttl != 7) { LeaveCriticalSection(&pool->lock); return false; }
        ((ICMP_ECHO_REPLY *)queries[i].reply)->Status = IP_REQ_TIMED_OUT;
        SetEvent(queries[i].event);
    }
    SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
    ULONGLONG deadline = GetTickCount64()+2000;
    while (pending_count(pool)) { if (GetTickCount64() > deadline) return false; Sleep(1); }
    return true;
}
int main(void) {
    win_ping_pool_t pool = {0}; lan_host_t host = {0};
    CHECK(win_ping_init(&pool) && !win_ping_init(&pool));
    for (mode = SYNC_REPLY; mode <= ASYNC_REPLY; ++mode) {
        CHECK(win_ping_probe(&pool,1,2,5000,&host,never_cancel,NULL) == SCAN_OK);
        CHECK(host.has_latency && host.latency_ms == 123 && host.ttl == 64 && !pending_count(&pool));
    }
    mode = FAILED_REPLY; host = (lan_host_t){0};
    CHECK(win_ping_probe(&pool,1,2,5000,&host,never_cancel,NULL) == SCAN_UNREACHABLE && !host.has_latency);
    mode = SYNC_REPLY; reply_status = IP_TTL_EXPIRED_TRANSIT; win_echo_result_t echo;
    CHECK(win_ping_probe_ttl(&pool,1,2,5000,7,&host,&echo,never_cancel,NULL) == SCAN_UNREACHABLE && echo.received && echo.status == IP_TTL_EXPIRED_TRANSIT && echo.address == 0xc0a80101 && echo.latency_ms == 123 && last_ttl == 7 && !host.has_latency);
    reply_status = IP_SUCCESS; mode = PENDING_REPLY; checks = 0;
    CHECK(win_ping_probe_ttl(&pool,1,2,5000,7,&host,&echo,cancel_soon,NULL) == SCAN_CANCELLED && complete_pending(&pool));
    mode = PENDING_REPLY;
    ULONGLONG started = GetTickCount64(); checks = 0;
    CHECK(win_ping_probe(&pool,1,2,5000,&host,cancel_soon,NULL) == SCAN_CANCELLED && !host.has_latency);
    CHECK(GetTickCount64()-started < 200 && pending_count(&pool) == 1);
    CHECK(complete_pending(&pool));
    DWORD before, after; GetProcessHandleCount(GetCurrentProcess(),&before);
    for (unsigned batch = 0; batch < 10; ++batch) {
        for (unsigned i = 0; i < 32; ++i) { checks = 0; CHECK(win_ping_probe(&pool,1,2,5000,&host,cancel_soon,NULL) == SCAN_CANCELLED); }
        CHECK(complete_pending(&pool));
    }
    GetProcessHandleCount(GetCurrentProcess(),&after); CHECK(before == after);
    for (unsigned i = 0; i < PING_CAPACITY; ++i) { checks = 0; CHECK(win_ping_probe(&pool,1,2,5000,&host,cancel_soon,NULL) == SCAN_CANCELLED); }
    CHECK(pending_count(&pool) == PING_CAPACITY); checks = 0;
    CHECK(win_ping_probe(&pool,1,2,5000,&host,cancel_soon,NULL) == SCAN_CANCELLED && pending_count(&pool) == PING_CAPACITY);
    win_ping_stop(&pool); CHECK(!win_ping_release(&pool));
    CHECK(win_ping_probe(&pool,1,2,5000,&host,never_cancel,NULL) == SCAN_CANCELLED);
    CHECK(complete_pending(&pool));
    ULONGLONG deadline = GetTickCount64()+2000;
    while (!win_ping_release(&pool)) { CHECK(GetTickCount64() < deadline); Sleep(1); }
    CHECK(win_ping_release(&pool) && !premature_close && created == closed);
    CHECK(win_ping_init(&pool)); win_ping_stop(&pool);
    deadline = GetTickCount64()+2000;
    while (!win_ping_release(&pool)) { CHECK(GetTickCount64() < deadline); Sleep(1); }
    printf("Ping tests passed: sync/async/error, prompt cancellation, buffer lifetime, bounded 256 requests, delayed shutdown and 320 retire/reap cycles; handles %lu -> %lu.\n",before,after);
    return 0;
}
