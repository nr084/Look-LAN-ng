#ifndef NETWORK_H
#define NETWORK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint8_t family; uint8_t bytes[16]; uint32_t scope_id; } ip_address_t;
typedef struct { uint32_t first, last; } ipv4_range_t;
#define NETWORK_MAX_RANGES 64
bool ipv4_parse(const char *text, size_t length, uint32_t *value);
void ipv4_format(uint32_t value, char *text, size_t capacity);
bool network_range(uint32_t address, unsigned prefix, ipv4_range_t *range);
ip_address_t ipv4_address(uint32_t value);
uint32_t ipv4_value(const ip_address_t *address);
bool range_parse(const char *text, size_t length, ipv4_range_t *range);
bool ranges_normalize(ipv4_range_t *ranges, size_t *count, uint64_t *addresses);
bool ranges_contains(const ipv4_range_t *ranges, size_t count, uint32_t address);
bool ranges_address_at(const ipv4_range_t *ranges, size_t count, uint32_t offset, uint32_t *address);
#endif
