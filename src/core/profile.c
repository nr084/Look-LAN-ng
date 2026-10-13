#include "profile.h"
#include "ports.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static bool fail(char *s, size_t n, const char *message) { if (s && n) snprintf(s,n,"%s",message); return false; }
static bool utf8_valid(const char *s, size_t n) {
    for (size_t i = 0; i < n;) {
        uint32_t c = (uint8_t)s[i++]; unsigned extra; uint32_t minimum;
        if (!c) return false; if (c < 0x80) continue;
        if (c >= 0xc2 && c <= 0xdf) { extra = 1; c &= 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; c &= 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; c &= 7; minimum = 0x10000; } else return false;
        if (extra > n-i) return false;
        while (extra--) { uint8_t next = (uint8_t)s[i++]; if ((next&0xc0) != 0x80) return false; c = (c<<6)|(next&63); }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
    }
    return true;
}
static bool bounded_string(const char *s, size_t capacity) { const char *end = memchr(s,0,capacity); return end && utf8_valid(s,(size_t)(end-s)); }
void profile_defaults(profile_t *p) {
    memset(p,0,sizeof(*p)); memcpy(p->name,"Default",8);
    p->icmp_timeout_ms = 300; p->tcp_timeout_ms = 250; p->monitor_interval_sec = 30; p->concurrency = 64;
    p->port_count = discovery_port_count; memcpy(p->ports,discovery_ports,p->port_count*sizeof(*p->ports));
}
bool profile_validate(const profile_t *p, char *error, size_t cap) {
    if (!p || !p->name[0] || !bounded_string(p->name,sizeof(p->name)) || !bounded_string(p->interface_id,sizeof(p->interface_id)) || !bounded_string(p->interface_name,sizeof(p->interface_name)) || !bounded_string(p->notes,sizeof(p->notes))) return fail(error,cap,"Invalid or oversized UTF-8 name/notes/interface");
    for (size_t i = 0; p->name[i]; ++i) if ((uint8_t)p->name[i] < 32 || p->name[i] == 127) return fail(error,cap,"Profile name must be a single line without control characters");
    if (!p->range_count || p->range_count > NETWORK_MAX_RANGES) return fail(error,cap,"Specify 1-64 IPv4 ranges");
    for (size_t i = 0; i < p->range_count; ++i) if (p->ranges[i].last < p->ranges[i].first) return fail(error,cap,"Invalid IPv4 range");
    if (!p->port_count || p->port_count > 65535) return fail(error,cap,"Specify at least one TCP port");
    for (size_t i = 0; i < p->port_count; ++i) if (!p->ports[i] || (i && p->ports[i] <= p->ports[i-1])) return fail(error,cap,"Ports must be unique and sorted");
    if (p->icmp_timeout_ms < 50 || p->icmp_timeout_ms > 5000 || p->tcp_timeout_ms < 25 || p->tcp_timeout_ms > 5000) return fail(error,cap,"ICMP timeout: 50-5000 ms; TCP timeout: 25-5000 ms");
    if (p->concurrency < 8 || p->concurrency > 256) return fail(error,cap,"Concurrency must be 8-256");
    if (p->monitor_interval_sec != 5 && p->monitor_interval_sec != 10 && p->monitor_interval_sec != 30 && p->monitor_interval_sec != 60 && p->monitor_interval_sec != 300) return fail(error,cap,"Monitoring interval: 5, 10, 30, 60 or 300 seconds");
    return true;
}
static bool decode(const char *s, size_t n, char *out, size_t cap) {
    size_t used = 0;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i]; if (c == '\\') { if (++i == n) return false; c = s[i]; if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c != '\\') return false; }
        if (used+1 >= cap) return false; out[used++] = c;
    }
    out[used] = 0; return true;
}
static bool key_is(const char *s, size_t n, const char *key) { return n == strlen(key) && !memcmp(s,key,n); }
static bool integer(const char *s, size_t n, uint32_t *out) {
    uint32_t v = 0; if (!n || n > 10) return false;
    for (size_t i = 0; i < n; ++i) { if (s[i] < '0' || s[i] > '9') return false; unsigned d = (unsigned)(s[i]-'0'); if (v > (UINT32_MAX-d)/10) return false; v = v*10+d; }
    *out = v; return true;
}
bool profile_parse(const char *data, size_t n, profile_t *out, char *error, size_t cap) {
    if (!data || !out || !n || n > 65536 || !utf8_valid(data,n)) return fail(error,cap,"Profile must be valid UTF-8 text, at most 64 KiB");
    profile_t *p = malloc(sizeof(*p)); if (!p) return fail(error,cap,"Out of memory"); profile_defaults(p);
    size_t pos = n >= 3 && !memcmp(data,"\xef\xbb\xbf",3) ? 3 : 0; bool header = false, ok = true; uint32_t seen = 0;
    while (pos < n && ok) {
        size_t begin = pos; while (pos < n && data[pos] != '\n') ++pos; size_t end = pos; if (pos < n) ++pos;
        if (end > begin && data[end-1] == '\r') --end;
        while (begin < end && (data[begin] == ' ' || data[begin] == '\t')) ++begin;
        size_t nonblank = end; while (nonblank > begin && (data[nonblank-1] == ' ' || data[nonblank-1] == '\t')) --nonblank;
        if (begin == nonblank || data[begin] == '#') continue;
        const char *s = data+begin; size_t length = end-begin;
        if (!header) { header = key_is(s,nonblank-begin,"Look@LAN-ngProfile=1"); if (!header) ok = fail(error,cap,"Unsupported profile header/version"); continue; }
        const char *equal = memchr(s,'=',length); if (!equal) { ok = fail(error,cap,"Expected key=value"); break; }
        size_t key_n = (size_t)(equal-s), value_n = length-key_n-1; const char *value = equal+1; unsigned field = 0;
        if (key_is(s,key_n,"range")) { if (p->range_count == NETWORK_MAX_RANGES || !range_parse(value,value_n,&p->ranges[p->range_count])) ok = fail(error,cap,"Invalid or excessive range"); else ++p->range_count; continue; }
        if (key_is(s,key_n,"name")) field = 1; else if (key_is(s,key_n,"interface_id")) field = 2; else if (key_is(s,key_n,"interface_name")) field = 3; else if (key_is(s,key_n,"notes")) field = 4;
        else if (key_is(s,key_n,"ports")) field = 5; else if (key_is(s,key_n,"icmp_timeout_ms")) field = 6; else if (key_is(s,key_n,"tcp_timeout_ms")) field = 7; else if (key_is(s,key_n,"monitor_interval_sec")) field = 8; else if (key_is(s,key_n,"concurrency")) field = 9;
        else { ok = fail(error,cap,"Unknown profile key"); break; }
        if (seen & (1u<<field)) { ok = fail(error,cap,"Duplicate profile key"); break; } seen |= 1u<<field;
        if (field == 1) ok = decode(value,value_n,p->name,sizeof(p->name)); else if (field == 2) ok = decode(value,value_n,p->interface_id,sizeof(p->interface_id)); else if (field == 3) ok = decode(value,value_n,p->interface_name,sizeof(p->interface_name)); else if (field == 4) ok = decode(value,value_n,p->notes,sizeof(p->notes));
        else if (field == 5) ok = ports_parse(value,value_n,p->ports,65535,&p->port_count);
        else { uint32_t v; ok = integer(value,value_n,&v); if (ok) { if (field == 6) p->icmp_timeout_ms = v; else if (field == 7) p->tcp_timeout_ms = v; else if (field == 8) p->monitor_interval_sec = v; else p->concurrency = v; } }
        if (!ok) fail(error,cap,"Invalid or oversized profile value");
    }
    if (ok && !header) ok = fail(error,cap,"Missing profile header");
    if (ok) ok = profile_validate(p,error,cap);
    uint64_t total; if (ok) ok = ranges_normalize(p->ranges,&p->range_count,&total);
    if (ok) *out = *p;
    free(p); return ok;
}
bool profile_ports_text(const profile_t *p, char *text, size_t cap) {
    size_t used = 0; if (!text || !cap || !p->port_count || p->port_count > 65535) return false;
    for (size_t i = 0; i < p->port_count;) {
        uint16_t first = p->ports[i], last = first; while (i+1 < p->port_count && (unsigned)p->ports[i+1] == (unsigned)last+1) last = p->ports[++i];
        int n = first == last ? snprintf(text+used,cap-used,"%s%u",used ? "," : "",first) : snprintf(text+used,cap-used,"%s%u-%u",used ? "," : "",first,last);
        if (n < 0 || (size_t)n >= cap-used) return false; used += (size_t)n; ++i;
    }
    return true;
}
bool profile_ranges_text(const profile_t *p, char *text, size_t cap) {
    size_t used = 0; if (!text || !cap || p->range_count > NETWORK_MAX_RANGES) return false; text[0] = 0;
    for (size_t i = 0; i < p->range_count; ++i) { char first[32], last[32]; ipv4_format(p->ranges[i].first,first,32); ipv4_format(p->ranges[i].last,last,32); int n = snprintf(text+used,cap-used,"%s%s-%s",used ? "\r\n" : "",first,last); if (n < 0 || (size_t)n >= cap-used) return false; used += (size_t)n; }
    return true;
}
static bool write_text(text_writer_t writer, void *context, const char *s) { return writer(context,s,strlen(s)); }
static bool write_field(text_writer_t writer, void *ctx, const char *key, const char *s) {
    if (!write_text(writer,ctx,key) || !write_text(writer,ctx,"=")) return false;
    for (; *s; ++s) { const char *escape = *s == '\\' ? "\\\\" : *s == '\n' ? "\\n" : *s == '\r' ? "\\r" : NULL; if (escape ? !write_text(writer,ctx,escape) : !writer(ctx,s,1)) return false; }
    return write_text(writer,ctx,"\n");
}
typedef struct {text_writer_t writer;void *context;size_t count;} profile_output_t;
static bool limited_profile_write(void *context,const char *data,size_t length){
    profile_output_t *output=context;if(length>65536-output->count)return false;
    if(!output->writer(output->context,data,length))return false;
    output->count+=length;return true;
}
bool profile_write(const profile_t *p, text_writer_t writer, void *ctx) {
    if (!writer || !profile_validate(p,NULL,0)) return false;
    profile_output_t output={writer,ctx,0};writer=limited_profile_write;ctx=&output;
    char ports[PROFILE_PORT_TEXT_CAPACITY], buffer[256]; if (!profile_ports_text(p,ports,sizeof(ports))) return false;
    if (!write_text(writer,ctx,"Look@LAN-ngProfile=1\n") || !write_field(writer,ctx,"name",p->name) || !write_field(writer,ctx,"interface_id",p->interface_id) || !write_field(writer,ctx,"interface_name",p->interface_name)) return false;
    for (size_t i = 0; i < p->range_count; ++i) { char first[32], last[32]; ipv4_format(p->ranges[i].first,first,32); ipv4_format(p->ranges[i].last,last,32); snprintf(buffer,sizeof(buffer),"range=%s-%s\n",first,last); if (!write_text(writer,ctx,buffer)) return false; }
    snprintf(buffer,sizeof(buffer),"icmp_timeout_ms=%u\ntcp_timeout_ms=%u\nmonitor_interval_sec=%u\nconcurrency=%u\n",p->icmp_timeout_ms,p->tcp_timeout_ms,p->monitor_interval_sec,p->concurrency);
    return write_field(writer,ctx,"ports",ports) && write_text(writer,ctx,buffer) && write_field(writer,ctx,"notes",p->notes);
}
