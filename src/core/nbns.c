#include "nbns.h"
#include <string.h>
static uint16_t word(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
bool nbns_request(uint16_t transaction, uint8_t *p, size_t cap) {
    if (!p || cap < 50) return false;
    memset(p,0,50); p[0] = (uint8_t)(transaction>>8); p[1] = (uint8_t)transaction; p[5] = 1; p[12] = 32;
    for (size_t i = 0; i < 16; ++i) { uint8_t value = i ? 0 : '*'; p[13+i*2] = (uint8_t)('A'+(value>>4)); p[14+i*2] = (uint8_t)('A'+(value&15)); }
    p[47] = 0x21; p[49] = 1; return true;
}
static bool skip_name(const uint8_t *p, size_t n, size_t *at) {
    for (size_t labels = 0; labels < 128 && *at < n; ++labels) {
        unsigned length = p[(*at)++];
        if (!length) return true;
        if ((length&0xc0) == 0xc0) { if (*at >= n) return false; size_t pointer = ((length&63)<<8)|p[(*at)++]; return pointer < n; }
        if (length > 63 || length > n-*at) return false;
        *at += length;
    }
    return false;
}
bool nbns_name(const uint8_t *p, size_t n, uint16_t transaction, char name[16]) {
    if (!p || !name || n < 12 || word(p) != transaction || !(word(p+2)&0x8000) || (word(p+2)&0x7a0f)) return false;
    unsigned questions = word(p+4), answers = word(p+6); if (questions > 8 || !answers || answers > 8) return false;
    size_t at = 12;
    for (unsigned i = 0; i < questions; ++i) { if (!skip_name(p,n,&at) || n-at < 4) return false; at += 4; }
    for (unsigned i = 0; i < answers; ++i) {
        if (!skip_name(p,n,&at) || n-at < 10) return false;
        uint16_t type = word(p+at), cls = word(p+at+2), bytes = word(p+at+8); at += 10;
        if (bytes > n-at) return false;
        if (type == 0x21 && cls == 1 && bytes) {
            unsigned names = p[at]; if ((size_t)names*18+1 > bytes) return false;
            for (unsigned j = 0; j < names; ++j) {
                const uint8_t *record = p+at+1+j*18;
                if (record[15] || (word(record+16)&0x8000)) continue;
                size_t length = 15; while (length && record[length-1] == ' ') --length;
                bool valid = length != 0; for (size_t k = 0; k < length; ++k) if (record[k] < 32 || record[k] == 127) valid = false;
                if (!valid) continue; memcpy(name,record,length); name[length] = 0; return true;
            }
        }
        at += bytes;
    }
    return false;
}
