#ifndef POSIX_NET_H
#define POSIX_NET_H
#include "scanner.h"
#include "profile.h"
#include <pthread.h>
#include <stdatomic.h>
#define POSIX_INTERFACES 64
#define POSIX_EVENTS 512
typedef struct { char name[128]; uint32_t index, ip,gateway,dns[3],metric;size_t dns_count;unsigned prefix; uint8_t mac[6]; bool has_mac,automatic,has_metric; } posix_interface_t;
typedef enum { PX_HOST, PX_CACHE, PX_NAME, PX_PING, PX_PROGRESS, PX_DONE, PX_ERROR } posix_event_type_t;
enum {PX_PROOF_ARP=1,PX_PROOF_IP=2,PX_PROOF_LOCAL=4};
typedef struct { posix_event_type_t type; lan_host_t host; uint32_t completed,total,proof;uint64_t generation; int error; } posix_event_t;
typedef struct {uint32_t ip;uint64_t generation;posix_interface_t nic;uint8_t mac[6];bool has_mac;} posix_name_request_t;
typedef struct {uint32_t ip,interface_index,source;uint64_t expires;uint8_t mac[6];bool has_mac;char name[256];} posix_name_cache_t;
typedef struct posix_backend {
    pthread_mutex_t lock; pthread_t coordinator;
    atomic_bool cancel, running,system_error,shutdown; bool joinable,monitoring;atomic_uint pace_ms;atomic_uint_fast64_t generation;struct posix_backend *owner;uint64_t provider_generation;
    posix_interface_t selected; profile_t profile;
    uint32_t next,total,completed; uint64_t ticket;
    posix_event_t events[POSIX_EVENTS]; size_t head,count;
    posix_name_request_t names[4096];size_t name_head,name_count;bool providers_started;pthread_t providers[2];size_t provider_count;atomic_uint providers_alive,providers_busy;
    posix_name_cache_t name_cache[1024];size_t cache_next;
    int neighbor_fd;atomic_bool neighbor_done;uint8_t neighbor_mac[65536][6];bool neighbor_live[65536],neighbor_requested[65536],neighbor_proved[65536],name_queued[65536];uint8_t name_mac[65536][6];
} posix_backend_t;
size_t posix_interfaces(posix_interface_t *out, size_t capacity);
bool posix_backend_init(posix_backend_t *b);
bool posix_scan_start(posix_backend_t *b, const posix_interface_t *selected, const profile_t *profile);
void posix_stop(posix_backend_t *b);
bool posix_pop(posix_backend_t *b, posix_event_t *event);
/* Call on the UI timer; never joins while the coordinator is running. */
bool posix_reap(posix_backend_t *b);
bool posix_backend_destroy(posix_backend_t *b);
bool posix_publish(posix_backend_t *backend,posix_event_t event);
bool posix_pace(posix_backend_t *backend);
bool posix_cancelled(const posix_backend_t *backend);
void posix_invalidate_names(posix_backend_t *backend);
bool posix_names_pending(posix_backend_t *backend);
bool posix_target_slot(const posix_backend_t *backend,uint32_t ip,size_t *slot);
void posix_queue_name(posix_backend_t *backend,uint32_t ip);
#endif
