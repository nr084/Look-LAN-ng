#ifndef VENDOR_H
#define VENDOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { const uint8_t *data; size_t length; uint32_t count, string_bytes; } vendor_table_t;
bool vendor_open(vendor_table_t *table, const void *data, size_t length);
/* Returns a borrowed immutable UTF-8 string, NULL for unknown/local MAC. */
const char *vendor_lookup(const vendor_table_t *table, const uint8_t mac[6]);
#endif
