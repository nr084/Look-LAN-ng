#include "report.h"
#include "ports.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static bool text(text_writer_t w, void *ctx, const char *s) { return w(ctx,s,strlen(s)); }
static size_t bounded_length(const char *s, size_t cap) { size_t n = 0; while (n < cap && s[n]) ++n; return n; }
static bool escaped(report_format_t f, text_writer_t w, void *ctx, const char *s, size_t length) {
    if (f == REPORT_CSV) {
        if (!text(w,ctx,"\"")) return false;
        if (length && (s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@' || s[0] == '\t' || s[0] == '\r' || s[0] == '\n') && !text(w,ctx,"'")) return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const char *replacement = NULL;
        if (f == REPORT_HTML) {
            if (s[i] == '&') replacement = "&amp;"; else if (s[i] == '<') replacement = "&lt;"; else if (s[i] == '>') replacement = "&gt;"; else if (s[i] == '"') replacement = "&quot;"; else if (s[i] == '\'') replacement = "&#39;";
        } else if (f == REPORT_CSV && s[i] == '"') replacement = "\"\"";
        else if (f == REPORT_TXT && (s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) replacement = " ";
        if (replacement ? !text(w,ctx,replacement) : !w(ctx,s+i,1)) return false;
    }
    return f != REPORT_CSV || text(w,ctx,"\"");
}
static void utc_timestamp(uint64_t seconds, char *out, size_t cap) {
    if (!seconds || seconds > INT64_MAX) { snprintf(out,cap,"—"); return; }
    time_t value = (time_t)seconds; struct tm *utc = gmtime(&value);
    if (!utc || !strftime(out,cap,"%Y-%m-%d %H:%M:%S",utc)) snprintf(out,cap,"—");
}
static const char *status(const lan_host_t *h, bool ru) {
    if (!h->first_seen) return ru ? "НЕИЗВЕСТНО" : "UNKNOWN";
    if (!h->online) return ru ? "НЕ В СЕТИ" : "OFFLINE";
    if (h->new_device) return ru ? "НОВОЕ" : "NEW";
    if (h->changed) return ru ? "ИЗМЕНЕНО" : "CHANGED";
    return ru ? "В СЕТИ" : "ONLINE";
}
typedef struct { char ip[64], mac[32], device[128], latency[32], services[8192], first[48], last[48]; } row_t;
static bool write_row(report_format_t f, text_writer_t w, void *ctx, const char *const *fields, const size_t *lengths, bool heading) {
    if (f == REPORT_HTML && !text(w,ctx,"<tr>")) return false;
    for (unsigned i = 0; i < 10; ++i) {
        if (f == REPORT_HTML) { if (!text(w,ctx,heading ? "<th>" : "<td>")) return false; }
        else if (i && !text(w,ctx,f == REPORT_CSV ? "," : "\t")) return false;
        if (!escaped(f,w,ctx,fields[i],lengths[i])) return false;
        if (f == REPORT_HTML && !text(w,ctx,heading ? "</th>" : "</td>")) return false;
    }
    return text(w,ctx,f == REPORT_HTML ? "</tr>\n" : "\r\n");
}
bool report_write(report_format_t format, const lan_host_t *hosts, size_t count, const vendor_table_t *vendors, const report_options_t *o, text_writer_t w, void *ctx) {
    if (!w || !o || (count && !hosts) || format < REPORT_CSV || format > REPORT_HTML || !memchr(o->profile_name,0,sizeof(o->profile_name)) || !memchr(o->copyright,0,sizeof(o->copyright))) return false;
    const char *en[] = {"Status","IP Address","Host Name","MAC Address","Vendor","Device / OS","Latency (ms)","Services","First Seen (UTC)","Last Seen (UTC)"};
    const char *ru[] = {"Статус","IP-адрес","Имя узла","MAC-адрес","Производитель","Устройство / ОС","Задержка (мс)","Сервисы","Первое обнаружение (UTC)","Последний ответ (UTC)"};
    const char **headings = o->russian ? ru : en; size_t lengths[10]; for (unsigned i = 0; i < 10; ++i) lengths[i] = strlen(headings[i]);
    if (format == REPORT_HTML) {
        if (!text(w,ctx,"<!doctype html><html lang=\"") || !text(w,ctx,o->russian ? "ru" : "en") || !text(w,ctx,"\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>Look@LAN-ng</title><style>body{font:13px system-ui,Segoe UI,sans-serif;margin:20px;color:#24303c;background:#fff}h1{font-size:20px}table{border-collapse:collapse;width:100%}th,td{padding:6px 8px;border:1px solid #d7dee5;text-align:left;vertical-align:top}th{background:#eef2f6}tbody tr:nth-child(even){background:#fafbfd}p{color:#52606d}</style></head><body><h1>Look@LAN-ng</h1><p>")) return false;
        if (!escaped(format,w,ctx,o->profile_name,strlen(o->profile_name)) || !text(w,ctx," · ") || !escaped(format,w,ctx,o->copyright,strlen(o->copyright)) || !text(w,ctx,"</p><p>") || !text(w,ctx,o->russian ? "Названия сервисов — подсказки по номерам TCP-портов. Время — UTC." : "Service names are TCP port hints. Timestamps are UTC.") || !text(w,ctx,"</p><table><thead>")) return false;
    } else if (format == REPORT_CSV) { if (!text(w,ctx,"\xef\xbb\xbf")) return false; }
    else { if (!text(w,ctx,"Look@LAN-ng\r\n") || !escaped(format,w,ctx,o->profile_name,strlen(o->profile_name)) || !text(w,ctx,"\r\n") || !text(w,ctx,o->copyright) || !text(w,ctx,"\r\n\r\n")) return false; }
    if (!write_row(format,w,ctx,headings,lengths,true)) return false;
    if (format == REPORT_HTML && !text(w,ctx,"</thead><tbody>\n")) return false;
    for (size_t i = 0; i < count; ++i) {
        const lan_host_t *h = &hosts[i]; row_t row = {0};
        if (h->address.family == 4) ipv4_format(ipv4_value(&h->address),row.ip,sizeof(row.ip));
        else if (h->address.family == 6) snprintf(row.ip,sizeof(row.ip),"%x:%x:%x:%x:%x:%x:%x:%x",((unsigned)h->address.bytes[0]<<8)|h->address.bytes[1],((unsigned)h->address.bytes[2]<<8)|h->address.bytes[3],((unsigned)h->address.bytes[4]<<8)|h->address.bytes[5],((unsigned)h->address.bytes[6]<<8)|h->address.bytes[7],((unsigned)h->address.bytes[8]<<8)|h->address.bytes[9],((unsigned)h->address.bytes[10]<<8)|h->address.bytes[11],((unsigned)h->address.bytes[12]<<8)|h->address.bytes[13],((unsigned)h->address.bytes[14]<<8)|h->address.bytes[15]);
        if (h->has_mac) snprintf(row.mac,sizeof(row.mac),"%02X:%02X:%02X:%02X:%02X:%02X",h->mac[0],h->mac[1],h->mac[2],h->mac[3],h->mac[4],h->mac[5]);
        const char *vendor = h->has_mac ? vendor_lookup(vendors,h->mac) : NULL;
        if (!vendor) vendor = h->has_mac && (h->mac[0]&2) ? (o->russian ? "Локальный / приватный MAC" : "Local / private MAC") : (o->russian ? "Неизвестно" : "Unknown");
        const char *family = h->os_family == OS_WINDOWS ? "Windows" : h->os_family == OS_LINUX ? "Linux/Unix" : h->os_family == OS_MACOS ? "macOS" : h->os_family == OS_BSD ? "BSD" : h->os_family == OS_PRINTER ? (o->russian ? "Принтер" : "Printer") : h->os_family == OS_NAS ? "NAS" : h->os_family == OS_NETWORK_DEVICE ? (o->russian ? "Сетевое устройство" : "Network device") : h->os_family == OS_EMBEDDED ? (o->russian ? "Встроенное устройство" : "Embedded") : (o->russian ? "Неизвестно" : "Unknown");
        if (h->os_family != OS_UNKNOWN) snprintf(row.device,sizeof(row.device),"%s — %s (%u%%)",family,o->russian ? "вероятно" : "probable",h->os_confidence); else snprintf(row.device,sizeof(row.device),"%s",family);
        if (h->has_latency) snprintf(row.latency,sizeof(row.latency),"%u",h->latency_ms);
        size_t used = 0;
        for (unsigned p = 0; p < h->port_count && p < 64; ++p) { int n = snprintf(row.services+used,sizeof(row.services)-used,"%s%s:%u",used ? ", " : "",port_service(h->open_ports[p]),h->open_ports[p]); if (n < 0 || (size_t)n >= sizeof(row.services)-used) return false; used += (size_t)n; }
        if (h->ports_truncated && used+8 < sizeof(row.services)) memcpy(row.services+used,", ...",6);
        utc_timestamp(h->first_seen,row.first,sizeof(row.first)); utc_timestamp(h->last_seen,row.last,sizeof(row.last));
        const char *fields[] = {status(h,o->russian),row.ip,h->hostname,row.mac,vendor,row.device,row.latency,row.services,row.first,row.last};
        for (unsigned c = 0; c < 10; ++c) lengths[c] = c == 2 ? bounded_length(h->hostname,sizeof(h->hostname)) : c == 4 ? bounded_length(vendor,1024) : strlen(fields[c]);
        if (!write_row(format,w,ctx,fields,lengths,false)) return false;
    }
    return format != REPORT_HTML || text(w,ctx,"</tbody></table></body></html>\n");
}
