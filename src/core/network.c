#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool ipv4_parse(const char *s, size_t n, uint32_t *out) {
    uint32_t value = 0;
    size_t pos = 0;
    if (!s || !out || !n) return false;
    for (unsigned part = 0; part < 4; ++part) {
        unsigned octet = 0, digits = 0;
        while (pos < n && s[pos] >= '0' && s[pos] <= '9') {
            octet = octet * 10 + (unsigned)(s[pos++] - '0');
            if (++digits > 3 || octet > 255) return false;
        }
        if (!digits) return false;
        value = (value << 8) | octet;
        if (part < 3 && (pos >= n || s[pos++] != '.')) return false;
    }
    if (pos != n) return false;
    *out = value;
    return true;
}
void ipv4_format(uint32_t v, char *s, size_t n) {
    if (s && n) snprintf(s, n, "%u.%u.%u.%u", v >> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255);
}
bool network_range(uint32_t ip, unsigned prefix, ipv4_range_t *r) {
    if (!r || prefix > 32) return false;
    uint32_t mask = prefix ? UINT32_MAX << (32 - prefix) : 0;
    r->first = ip & mask;
    r->last = r->first | ~mask;
    if (prefix < 31) { ++r->first; --r->last; }
    return true;
}
ip_address_t ipv4_address(uint32_t v) {
    ip_address_t a = {0};
    a.family = 4;
    for (unsigned i = 0; i < 4; ++i) a.bytes[i] = (uint8_t)(v >> (24 - 8 * i));
    return a;
}
uint32_t ipv4_value(const ip_address_t *a) {
    return ((uint32_t)a->bytes[0] << 24) | ((uint32_t)a->bytes[1] << 16) | ((uint32_t)a->bytes[2] << 8) | a->bytes[3];
}
static bool decimal(const char *s, size_t n, unsigned maximum, unsigned *out) {
    unsigned v = 0; if (!n) return false;
    for (size_t i = 0; i < n; ++i) { if (s[i] < '0' || s[i] > '9' || v > maximum/10) return false; v = v*10+(unsigned)(s[i]-'0'); if (v > maximum) return false; }
    *out = v; return true;
}
bool range_parse(const char *s, size_t n, ipv4_range_t *out) {
    if (!s || !out || !n || n > 128) return false;
    while (n && (*s == ' ' || *s == '\t')) { ++s; --n; }
    while (n && (s[n-1] == ' ' || s[n-1] == '\t')) --n;
    const char *slash = memchr(s,'/',n), *dash = memchr(s,'-',n); ipv4_range_t r;
    if (slash) {
        unsigned prefix; uint32_t ip;
        if (dash || !ipv4_parse(s,(size_t)(slash-s),&ip) || !decimal(slash+1,n-(size_t)(slash-s)-1,32,&prefix) || !network_range(ip,prefix,&r)) return false;
    } else if (dash) {
        if (!ipv4_parse(s,(size_t)(dash-s),&r.first)) return false;
        size_t length = n-(size_t)(dash-s)-1;
        if (memchr(dash+1,'.',length)) { if (!ipv4_parse(dash+1,length,&r.last)) return false; }
        else { unsigned last; if (!decimal(dash+1,length,255,&last)) return false; r.last = (r.first & 0xffffff00u)|last; }
        if (r.last < r.first) return false;
    } else { if (!ipv4_parse(s,n,&r.first)) return false; r.last = r.first; }
    *out = r; return true;
}
static int compare_range(const void *a, const void *b) {
    const ipv4_range_t *x = a, *y = b;
    return x->first != y->first ? (x->first > y->first)-(x->first < y->first) : (x->last > y->last)-(x->last < y->last);
}
bool ranges_normalize(ipv4_range_t *ranges, size_t *count, uint64_t *total) {
    if (!ranges || !count || !total || !*count || *count > NETWORK_MAX_RANGES) return false;
    for (size_t i = 0; i < *count; ++i) if (ranges[i].last < ranges[i].first) return false;
    qsort(ranges,*count,sizeof(*ranges),compare_range); size_t used = 0;
    for (size_t i = 0; i < *count; ++i) {
        if (used && (uint64_t)ranges[i].first <= (uint64_t)ranges[used-1].last+1) { if (ranges[i].last > ranges[used-1].last) ranges[used-1].last = ranges[i].last; }
        else ranges[used++] = ranges[i];
    }
    *total = 0; for (size_t i = 0; i < used; ++i) *total += (uint64_t)ranges[i].last-ranges[i].first+1;
    *count = used; return true;
}
bool ranges_contains(const ipv4_range_t *ranges, size_t n, uint32_t ip) {
    for (size_t i = 0; i < n; ++i) if (ip >= ranges[i].first && ip <= ranges[i].last) return true;
    return false;
}
bool ranges_address_at(const ipv4_range_t *ranges, size_t n, uint32_t offset, uint32_t *ip) {
    uint64_t remaining = offset;
    if (!ip) return false;
    for (size_t i = 0; i < n; ++i) { uint64_t size = (uint64_t)ranges[i].last-ranges[i].first+1; if (remaining < size) { *ip = ranges[i].first+(uint32_t)remaining; return true; } remaining -= size; }
    return false;
}
