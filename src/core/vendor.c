#include "vendor.h"
#include <string.h>
static uint32_t read32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
bool vendor_open(vendor_table_t *t, const void *data, size_t n) {
    memset(t,0,sizeof(*t));
    if (!data || n < 12 || memcmp(data,"OUI1",4)) return false;
    const uint8_t *p = data; uint32_t count = read32(p+4), strings = read32(p+8);
    if ((uint64_t)count*8 + 12 + strings != n) return false;
    const uint8_t *blob = p+12+(size_t)count*8;
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t key = read32(p+12+(size_t)i*8), offset = read32(p+16+(size_t)i*8);
        if (key > 0xffffff || (i && key <= previous) || offset >= strings || !memchr(blob+offset,0,strings-offset)) return false;
        previous = key;
    }
    t->data = p; t->length = n; t->count = count; t->string_bytes = strings; return true;
}
const char *vendor_lookup(const vendor_table_t *t, const uint8_t mac[6]) {
    if (!t || !t->data || !mac || (mac[0]&3)) return NULL;
    uint32_t key = ((uint32_t)mac[0]<<16) | ((uint32_t)mac[1]<<8) | mac[2];
    uint32_t low = 0, high = t->count;
    while (low < high) {
        uint32_t mid = low+(high-low)/2, value = read32(t->data+12+(size_t)mid*8);
        if (value < key) low = mid+1; else high = mid;
    }
    if (low == t->count || read32(t->data+12+(size_t)low*8) != key) return NULL;
    uint32_t offset = read32(t->data+16+(size_t)low*8);
    return (const char *)(t->data+12+(size_t)t->count*8+offset);
}
