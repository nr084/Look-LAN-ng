#ifndef PROFILE_H
#define PROFILE_H
#include "network.h"
#include <stdint.h>
typedef struct {
    char name[256], interface_id[128], interface_name[256], notes[2048];
    ipv4_range_t ranges[NETWORK_MAX_RANGES];
    size_t range_count;
    uint16_t ports[65535];
    size_t port_count;
    uint32_t icmp_timeout_ms, tcp_timeout_ms, monitor_interval_sec, concurrency;
} profile_t;
typedef bool (*text_writer_t)(void *context, const char *data, size_t length);
void profile_defaults(profile_t *profile);
bool profile_validate(const profile_t *profile, char *error, size_t error_capacity);
bool profile_parse(const char *data, size_t length, profile_t *profile, char *error, size_t error_capacity);
bool profile_write(const profile_t *profile, text_writer_t writer, void *context);
bool profile_ports_text(const profile_t *profile, char *text, size_t capacity);
bool profile_ranges_text(const profile_t *profile, char *text, size_t capacity);
#endif
