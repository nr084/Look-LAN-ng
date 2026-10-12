#ifndef WIN_PING_H
#define WIN_PING_H
#include <winsock2.h>
#include <windows.h>
#include "scanner.h"
#define PING_CAPACITY 256
typedef struct ping_request ping_request_t;
typedef struct {
    CRITICAL_SECTION lock;
    HANDLE wake, reaper;
    ping_request_t *requests[PING_CAPACITY];
    size_t count;
    bool initialized, stopping;
} win_ping_pool_t;
bool win_ping_init(win_ping_pool_t *pool);
void win_ping_stop(win_ping_pool_t *pool);
bool win_ping_release(win_ping_pool_t *pool);
typedef struct { uint32_t address, latency_ms, status; uint8_t ttl; bool received; } win_echo_result_t;
scan_result_t win_ping_probe_ttl(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t timeout, uint8_t ttl, lan_host_t *host, win_echo_result_t *echo, bool (*cancel)(void *), void *context);
scan_result_t win_ping_probe(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t timeout, lan_host_t *host, bool (*cancel)(void *), void *context);
#endif
