#ifndef MONITOR_H
#define MONITOR_H
#include "host.h"
typedef enum {
    MON_SCAN_STARTED, MON_SCAN_FINISHED, MON_SCAN_CANCELLED, MON_HOST_ONLINE,
    MON_HOST_OFFLINE, MON_NEW_HOST, MON_IP_CHANGED, MON_MAC_CHANGED,
    MON_HOSTNAME_CHANGED, MON_SCAN_ERROR, MON_EXPORT_STARTED, MON_EXPORT_FINISHED, MON_EXPORT_ERROR
} monitor_event_type_t;
typedef struct {
    uint64_t sequence, timestamp;
    monitor_event_type_t type;
    size_t host_index;
    ip_address_t address;
    char previous[256], current[256];
} monitor_event_t;
/* Model and ring storage belong to the caller and are touched by one thread. */
typedef struct {
    lan_host_t *hosts;
    size_t *count, capacity;
    monitor_event_t *log;
    size_t log_capacity, log_head, log_count;
    uint64_t sequence, round;
    ipv4_range_t ranges[NETWORK_MAX_RANGES];
    size_t range_count;
    uint32_t interface_index;
    bool baseline, mark_new, track_absent;
    uint8_t offline_threshold;
} monitor_t;
void monitor_init(monitor_t *model, lan_host_t *hosts, size_t *count, size_t capacity, monitor_event_t *log, size_t log_capacity);
void monitor_begin(monitor_t *model, ipv4_range_t range, uint32_t interface_index, bool track_absent, uint64_t now);
void monitor_begin_ranges(monitor_t *model, const ipv4_range_t *ranges, size_t count, uint32_t interface_index, bool track_absent, uint64_t now);
size_t monitor_observe(monitor_t *model, const lan_host_t *host, bool confirmed, uint64_t now);
void monitor_finish(monitor_t *model, bool complete, uint64_t now);
void monitor_note(monitor_t *model, monitor_event_type_t type, const char *detail, uint64_t now);
const monitor_event_t *monitor_log_at(const monitor_t *model, size_t chronological_index);
void monitor_acknowledge(monitor_t *model, size_t index);
#endif
