#include "win_neighbor.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <string.h>
struct neighbor_request {
    HANDLE event;
    uint32_t source, target, index;
    uint8_t mac[6];
    bool running, finished, abandoned, found;
};
static void free_request(neighbor_request_t *request) { CloseHandle(request->event); free(request); }
static DWORD WINAPI resolve_worker(void *context) {
    win_neighbor_pool_t *pool = context;
    for (;;) {
        WaitForSingleObject(pool->wake,INFINITE);
        EnterCriticalSection(&pool->lock);
        if (pool->stopping) { LeaveCriticalSection(&pool->lock); return 0; }
        size_t slot = NEIGHBOR_CAPACITY;
        for (size_t i = 0; i < NEIGHBOR_CAPACITY; ++i) if (pool->requests[i] && !pool->requests[i]->running && !pool->requests[i]->finished) { slot = i; break; }
        if (slot == NEIGHBOR_CAPACITY) { ResetEvent(pool->wake); LeaveCriticalSection(&pool->lock); continue; }
        neighbor_request_t *request = pool->requests[slot]; request->running = true;
        LeaveCriticalSection(&pool->lock);
        MIB_IPNET_ROW2 row = {0}; row.InterfaceIndex = request->index;
        row.Address.Ipv4.sin_family = AF_INET; row.Address.Ipv4.sin_addr.s_addr = htonl(request->target);
        SOCKADDR_INET source = {0}; source.Ipv4.sin_family = AF_INET; source.Ipv4.sin_addr.s_addr = htonl(request->source);
        DWORD error = ResolveIpNetEntry2(&row,&source);
        EnterCriticalSection(&pool->lock);
        request->found = error == NO_ERROR && row.PhysicalAddressLength == 6;
        if (request->found) memcpy(request->mac,row.PhysicalAddress,6);
        request->finished = true;
        if (request->abandoned) { pool->requests[slot] = NULL; free_request(request); }
        else SetEvent(request->event);
        LeaveCriticalSection(&pool->lock);
    }
}
bool win_neighbor_init(win_neighbor_pool_t *pool) {
    if (pool->initialized) return false;
    pool->stopping = false;
    memset(pool->requests,0,sizeof(pool->requests)); memset(pool->workers,0,sizeof(pool->workers));
    InitializeCriticalSection(&pool->lock);
    pool->wake = CreateEventW(NULL,TRUE,FALSE,NULL);
    if (!pool->wake) { DeleteCriticalSection(&pool->lock); return false; }
    pool->initialized = true;
    for (size_t i = 0; i < NEIGHBOR_WORKERS; ++i) {
        pool->workers[i] = CreateThread(NULL,0,resolve_worker,pool,0,NULL);
        if (!pool->workers[i]) { win_neighbor_stop(pool); while (!win_neighbor_release(pool)) Sleep(1); return false; }
    }
    return true;
}
void win_neighbor_stop(win_neighbor_pool_t *pool) {
    if (!pool->initialized) return;
    EnterCriticalSection(&pool->lock); pool->stopping = true; SetEvent(pool->wake); LeaveCriticalSection(&pool->lock);
}
bool win_neighbor_release(win_neighbor_pool_t *pool) {
    if (!pool->initialized) return true;
    for (size_t i = 0; i < NEIGHBOR_WORKERS; ++i) if (pool->workers[i] && WaitForSingleObject(pool->workers[i],0) != WAIT_OBJECT_0) return false;
    EnterCriticalSection(&pool->lock);
    bool busy = false; for (size_t i = 0; i < NEIGHBOR_CAPACITY; ++i) if (pool->requests[i]) busy = true;
    LeaveCriticalSection(&pool->lock);
    if (busy) return false;
    for (size_t i = 0; i < NEIGHBOR_WORKERS; ++i) if (pool->workers[i]) { CloseHandle(pool->workers[i]); pool->workers[i] = NULL; }
    CloseHandle(pool->wake); pool->wake = NULL; DeleteCriticalSection(&pool->lock); pool->initialized = false; return true;
}
scan_result_t win_neighbor_probe(win_neighbor_pool_t *pool, uint32_t source, uint32_t target, uint32_t index, uint8_t mac[6], bool (*cancel)(void *), void *context) {
    size_t slot = NEIGHBOR_CAPACITY; neighbor_request_t *request = NULL;
    if (!pool->initialized || !cancel || !mac) return SCAN_SYSTEM_ERROR;
    while (!request) {
        if (cancel(context)) return SCAN_CANCELLED;
        EnterCriticalSection(&pool->lock);
        if (pool->stopping) { LeaveCriticalSection(&pool->lock); return SCAN_CANCELLED; }
        for (size_t i = 0; i < NEIGHBOR_CAPACITY; ++i) if (!pool->requests[i]) { slot = i; break; }
        if (slot != NEIGHBOR_CAPACITY) {
            request = calloc(1,sizeof(*request));
            if (!request) { LeaveCriticalSection(&pool->lock); return SCAN_SYSTEM_ERROR; }
            request->event = CreateEventW(NULL,TRUE,FALSE,NULL);
            if (!request->event) { free(request); LeaveCriticalSection(&pool->lock); return SCAN_SYSTEM_ERROR; }
            request->source = source; request->target = target; request->index = index;
            pool->requests[slot] = request; SetEvent(pool->wake);
        }
        LeaveCriticalSection(&pool->lock);
        if (!request) Sleep(25);
    }
    for (;;) {
        bool stopped = cancel(context);
        WaitForSingleObject(request->event,stopped ? 0 : 25);
        EnterCriticalSection(&pool->lock);
        if (stopped || pool->stopping || request->finished) {
            scan_result_t result = stopped || pool->stopping ? SCAN_CANCELLED : request->found ? SCAN_OK : SCAN_UNREACHABLE;
            if (result == SCAN_OK) memcpy(mac,request->mac,6);
            if (!request->running || request->finished) { pool->requests[slot] = NULL; free_request(request); }
            else request->abandoned = true;
            LeaveCriticalSection(&pool->lock); return result;
        }
        LeaveCriticalSection(&pool->lock);
    }
}
