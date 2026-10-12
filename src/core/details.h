#ifndef DETAILS_H
#define DETAILS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define PING_HISTORY_CAPACITY 10000
#define DEVICE_NOTES_CAPACITY 1024
typedef struct { uint32_t ip, interface_index; uint64_t timestamp; uint32_t latency_ms, status, reply_ip; uint8_t hop; bool received; } ping_sample_t;
typedef struct { uint32_t ip, interface_index; char text[512]; } device_note_t;
typedef struct {
    ping_sample_t pings[PING_HISTORY_CAPACITY]; size_t ping_head, ping_count;
    device_note_t notes[DEVICE_NOTES_CAPACITY]; size_t note_count;
} details_store_t;
void details_reset(details_store_t *store);
void details_ping_add(details_store_t *store, ping_sample_t sample);
const ping_sample_t *details_ping_at(const details_store_t *store, size_t chronological_index);
const char *details_note(const details_store_t *store, uint32_t ip, uint32_t interface_index);
bool details_note_set(details_store_t *store, uint32_t ip, uint32_t interface_index, const char *text);
#endif
