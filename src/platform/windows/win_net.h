#ifndef WIN_NET_H
#define WIN_NET_H
#include <winsock2.h>
#include <windows.h>
#include "scanner.h"
#include "profile.h"
#include "win_ping.h"
#include "win_neighbor.h"
#define MAX_INTERFACES 64
#define EVENT_CAPACITY 2048
#define DNS_CAPACITY 4096
typedef struct {
    wchar_t name[128], description[256], dns[256];
    char id[128];
    uint32_t ip, gateway, index, metric;
    unsigned prefix;
    uint8_t mac[6];
    bool automatic, has_mac;
} win_interface_t;
typedef enum { NET_HOST, NET_DNS, NET_PROGRESS, NET_DONE, NET_ERROR, NET_PING } net_event_type_t;
typedef struct { net_event_type_t type; LONG generation; lan_host_t host; uint32_t completed; bool cancelled; DWORD error; } net_event_t;
typedef struct { lan_host_t host; LONG generation; uint32_t source; } dns_task_t;
typedef struct win_backend {
    CRITICAL_SECTION lock;
    HANDLE wake, coordinator, dns_threads[4];
    win_ping_pool_t ping;
    win_neighbor_pool_t neighbor;
    volatile LONG cancel, generation, completed, shutdown, dns_active;
    ULONGLONG next_probe;
    win_interface_t selected;
    ipv4_range_t range;
    ipv4_range_t ranges[NETWORK_MAX_RANGES];
    size_t range_count;
    uint32_t icmp_timeout_ms, tcp_timeout_ms, concurrency;
    net_event_t events[EVENT_CAPACITY];
    size_t head, count;
    dns_task_t dns_tasks[DNS_CAPACITY];
    size_t dns_head, dns_count;
    volatile LONG next;
    uint32_t total;
    uint16_t service_ports[65535];
    size_t service_count;
    wchar_t target[256];
    bool monitoring;
    uint32_t known_ips[65536];
    size_t known_count;
    bool initialized;
} win_backend_t;
DWORD win_enumerate(win_interface_t *interfaces, size_t capacity, size_t *count);
bool win_backend_init(win_backend_t *backend);
void win_backend_shutdown(win_backend_t *backend);
bool win_backend_release(win_backend_t *backend);
bool win_scan_start(win_backend_t *backend, const win_interface_t *selected, ipv4_range_t range);
bool win_scan_host_start(win_backend_t *backend, const win_interface_t *selected, const wchar_t *target, const uint16_t *ports, size_t count);
bool win_monitor_start(win_backend_t *backend, const win_interface_t *selected, ipv4_range_t range, const lan_host_t *hosts, size_t count);
bool win_profile_scan_start(win_backend_t *backend, const win_interface_t *selected, const profile_t *profile, bool monitoring, const lan_host_t *hosts, size_t count);
bool win_profile_host_start(win_backend_t *backend, const win_interface_t *selected, const wchar_t *target, const uint16_t *ports, size_t count, const profile_t *profile);
void win_scan_stop(win_backend_t *backend);
bool win_event_pop(win_backend_t *backend, net_event_t *event);
bool win_scan_running(win_backend_t *backend);
#endif
