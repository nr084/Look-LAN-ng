#ifndef WIN_NEIGHBOR_H
#define WIN_NEIGHBOR_H
#include <winsock2.h>
#include <windows.h>
#include "scanner.h"
#define NEIGHBOR_CAPACITY 256
#define NEIGHBOR_WORKERS 16
typedef struct neighbor_request neighbor_request_t;
typedef struct {
    CRITICAL_SECTION lock;
    HANDLE wake, workers[NEIGHBOR_WORKERS];
    neighbor_request_t *requests[NEIGHBOR_CAPACITY];
    bool initialized, stopping;
} win_neighbor_pool_t;
bool win_neighbor_init(win_neighbor_pool_t *pool);
void win_neighbor_stop(win_neighbor_pool_t *pool);
bool win_neighbor_release(win_neighbor_pool_t *pool);
scan_result_t win_neighbor_probe(win_neighbor_pool_t *pool, uint32_t source, uint32_t target, uint32_t index, uint8_t mac[6], bool (*cancel)(void *), void *context);
#endif
