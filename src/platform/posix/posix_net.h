#ifndef POSIX_NET_H
#define POSIX_NET_H
#include "scanner.h"
#include "profile.h"
#include <pthread.h>
#include <stdatomic.h>
#define POSIX_INTERFACES 64
#define POSIX_EVENTS 512
typedef struct { char name[128]; uint32_t index, ip; unsigned prefix; uint8_t mac[6]; bool has_mac; } posix_interface_t;
typedef enum { PX_HOST, PX_PROGRESS, PX_DONE, PX_ERROR } posix_event_type_t;
typedef struct { posix_event_type_t type; lan_host_t host; uint32_t completed,total; int error; } posix_event_t;
typedef struct {
    pthread_mutex_t lock; pthread_t coordinator;
    atomic_bool cancel, running; bool joinable;
    posix_interface_t selected; profile_t profile;
    uint32_t next,total,completed; uint64_t ticket;
    posix_event_t events[POSIX_EVENTS]; size_t head,count;
} posix_backend_t;
size_t posix_interfaces(posix_interface_t *out, size_t capacity);
bool posix_backend_init(posix_backend_t *b);
bool posix_scan_start(posix_backend_t *b, const posix_interface_t *selected, const profile_t *profile);
void posix_stop(posix_backend_t *b);
bool posix_pop(posix_backend_t *b, posix_event_t *event);
/* Call on the UI timer; never joins while the coordinator is running. */
bool posix_reap(posix_backend_t *b);
bool posix_backend_destroy(posix_backend_t *b);
#endif
