#include "win_ping.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdlib.h>
#include <string.h>
struct ping_request {
    HANDLE icmp, event;
    char payload[12];
    union { ICMP_ECHO_REPLY alignment; unsigned char bytes[sizeof(ICMP_ECHO_REPLY)+64]; } reply;
    bool retired;
    IP_OPTION_INFORMATION options;
};
static void dispose(ping_request_t *request) {
    IcmpCloseHandle(request->icmp); CloseHandle(request->event); free(request);
}
static DWORD WINAPI reap(void *context) {
    win_ping_pool_t *pool = context;
    DWORD wait = INFINITE;
    for (;;) {
        WaitForSingleObject(pool->wake,wait);
        EnterCriticalSection(&pool->lock);
        wait = INFINITE;
        for (size_t i = 0; i < PING_CAPACITY; ++i) {
            ping_request_t *request = pool->requests[i];
            if (!request || !request->retired) continue;
            if (WaitForSingleObject(request->event,0) != WAIT_OBJECT_0) { wait = 25; continue; }
            pool->requests[i] = NULL; --pool->count; dispose(request);
        }
        bool finished = pool->stopping && !pool->count;
        ResetEvent(pool->wake);
        LeaveCriticalSection(&pool->lock);
        if (finished) return 0;
    }
}
bool win_ping_init(win_ping_pool_t *pool) {
    if (pool->initialized) return false;
    pool->count = 0; pool->stopping = false;
    for (size_t i = 0; i < PING_CAPACITY; ++i) pool->requests[i] = NULL;
    InitializeCriticalSection(&pool->lock);
    pool->wake = CreateEventW(NULL,TRUE,FALSE,NULL);
    if (!pool->wake) { DeleteCriticalSection(&pool->lock); return false; }
    pool->reaper = CreateThread(NULL,0,reap,pool,0,NULL);
    if (!pool->reaper) { CloseHandle(pool->wake); pool->wake = NULL; DeleteCriticalSection(&pool->lock); return false; }
    pool->initialized = true; return true;
}
void win_ping_stop(win_ping_pool_t *pool) {
    if (!pool->initialized) return;
    EnterCriticalSection(&pool->lock); pool->stopping = true; SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
}
bool win_ping_release(win_ping_pool_t *pool) {
    if (!pool->initialized) return true;
    if (WaitForSingleObject(pool->reaper,0) != WAIT_OBJECT_0) return false;
    CloseHandle(pool->reaper); CloseHandle(pool->wake); pool->reaper = pool->wake = NULL;
    DeleteCriticalSection(&pool->lock); pool->initialized = false; return true;
}
scan_result_t win_ping_probe_ttl(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t timeout, uint8_t ttl, host_info_t *host, win_echo_result_t *echo, bool (*cancel)(void *), void *context) {
    if (echo) memset(echo,0,sizeof(*echo));
    if (!pool->initialized || !host || !cancel || !timeout || timeout > 5000) return SCAN_SYSTEM_ERROR;
    size_t slot = PING_CAPACITY;
    while (slot == PING_CAPACITY) {
        if (cancel(context)) return SCAN_CANCELLED;
        EnterCriticalSection(&pool->lock);
        if (pool->stopping) { LeaveCriticalSection(&pool->lock); return SCAN_CANCELLED; }
        for (size_t i = 0; i < PING_CAPACITY; ++i) if (!pool->requests[i]) { slot = i; break; }
        if (slot == PING_CAPACITY) { LeaveCriticalSection(&pool->lock); Sleep(25); continue; }
        /* Reserve the slot before issuing IO so concurrent probes remain bounded. */
        ping_request_t *request = calloc(1,sizeof(*request));
        if (!request) { LeaveCriticalSection(&pool->lock); return SCAN_SYSTEM_ERROR; }
        request->icmp = IcmpCreateFile(); request->event = CreateEventW(NULL,TRUE,FALSE,NULL);
        if (request->icmp == INVALID_HANDLE_VALUE || !request->event) {
            if (request->icmp != INVALID_HANDLE_VALUE) IcmpCloseHandle(request->icmp);
            if (request->event) CloseHandle(request->event);
            free(request); LeaveCriticalSection(&pool->lock); return SCAN_SYSTEM_ERROR;
        }
        memcpy(request->payload,"Look@LAN-ng",12);
        pool->requests[slot] = request; ++pool->count;
        LeaveCriticalSection(&pool->lock);
    }
    ping_request_t *request = pool->requests[slot];
    request->options.Ttl = ttl;
    DWORD count = IcmpSendEcho2Ex(request->icmp,request->event,NULL,NULL,htonl(source),htonl(target),request->payload,sizeof(request->payload),ttl ? &request->options : NULL,request->reply.bytes,sizeof(request->reply.bytes),timeout);
    DWORD error = count ? ERROR_SUCCESS : GetLastError();
    bool pending = count == ERROR_IO_PENDING || (!count && error == ERROR_IO_PENDING);
    if (pending) {
        for (;;) {
            if (cancel(context)) {
                EnterCriticalSection(&pool->lock); request->retired = true; SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
                return SCAN_CANCELLED;
            }
            DWORD wait = WaitForSingleObject(request->event,25);
            if (wait == WAIT_OBJECT_0) { count = IcmpParseReplies(request->reply.bytes,sizeof(request->reply.bytes)); error = count ? ERROR_SUCCESS : GetLastError(); break; }
            /* Do not free a possibly API-owned buffer on wait failure. */
            if (wait == WAIT_FAILED) {
                EnterCriticalSection(&pool->lock); request->retired = true; SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
                return SCAN_SYSTEM_ERROR;
            }
        }
    }
    scan_result_t result = SCAN_TIMEOUT;
    bool error_reply = !count && (error == IP_TTL_EXPIRED_TRANSIT || error == IP_TTL_EXPIRED_REASSEM || error == IP_DEST_HOST_UNREACHABLE || error == IP_DEST_NET_UNREACHABLE) && request->reply.alignment.Status == error && request->reply.alignment.Address;
    if (echo) { echo->received = count != 0 || error_reply; echo->status = count ? request->reply.alignment.Status : error; if (count || error_reply) { echo->address = ntohl(request->reply.alignment.Address); echo->latency_ms = request->reply.alignment.RoundTripTime; echo->ttl = request->reply.alignment.Options.Ttl; } }
    if (cancel(context)) result = SCAN_CANCELLED;
    else if (count && request->reply.alignment.Status == IP_SUCCESS) {
        host->has_latency = true; host->latency_ms = request->reply.alignment.RoundTripTime; host->ttl = request->reply.alignment.Options.Ttl; result = SCAN_OK;
    } else if (count || (error != IP_REQ_TIMED_OUT && error != ERROR_SUCCESS)) result = SCAN_UNREACHABLE;
    EnterCriticalSection(&pool->lock); pool->requests[slot] = NULL; --pool->count; SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
    dispose(request); return result;
}
scan_result_t win_ping_probe(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t timeout, host_info_t *host, bool (*cancel)(void *), void *context) {
    return win_ping_probe_ttl(pool,source,target,timeout,0,host,NULL,cancel,context);
}
