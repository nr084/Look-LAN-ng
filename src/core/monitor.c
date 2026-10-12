#include "monitor.h"
#include <string.h>
#include <stdio.h>
static void text_copy(char *out, const char *in) { size_t n = 0; if (in) while (n < 255 && in[n]) { out[n] = in[n]; ++n; } out[n] = 0; }
static void mac_text(const uint8_t *mac, char *text) { snprintf(text,256,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); }
static void add_event(monitor_t *m, monitor_event_type_t type, size_t index, const char *previous, const char *current, uint64_t now) {
    if (!m->log_capacity) return;
    size_t slot = (m->log_head+m->log_count)%m->log_capacity;
    if (m->log_count == m->log_capacity) { slot = m->log_head; m->log_head = (m->log_head+1)%m->log_capacity; }
    else ++m->log_count;
    monitor_event_t *event = &m->log[slot]; memset(event,0,sizeof(*event));
    event->sequence = ++m->sequence; event->timestamp = now; event->type = type; event->host_index = index;
    if (index < *m->count) event->address = m->hosts[index].address;
    text_copy(event->previous,previous); text_copy(event->current,current);
}
void monitor_init(monitor_t *m, lan_host_t *hosts, size_t *count, size_t cap, monitor_event_t *log, size_t log_cap) {
    memset(m,0,sizeof(*m)); m->hosts = hosts; m->count = count; m->capacity = cap; m->log = log; m->log_capacity = log_cap; m->offline_threshold = 2;
    *count = 0;
}
void monitor_begin(monitor_t *m, ipv4_range_t range, uint32_t interface_index, bool track_absent, uint64_t now) {
    monitor_begin_ranges(m,&range,1,interface_index,track_absent,now);
}
void monitor_begin_ranges(monitor_t *m, const ipv4_range_t *ranges, size_t count, uint32_t interface_index, bool track_absent, uint64_t now) {
    ++m->round; m->interface_index = interface_index; m->track_absent = track_absent; m->mark_new = m->baseline;
    m->range_count = ranges && count <= NETWORK_MAX_RANGES ? count : 0;
    if (m->range_count) memcpy(m->ranges,ranges,count*sizeof(*ranges)); else m->track_absent = false;
    char first[32], last[32], detail[128]; ipv4_format(m->range_count ? ranges[0].first : 0,first,32); ipv4_format(m->range_count ? ranges[0].last : 0,last,32); snprintf(detail,sizeof(detail),"%s - %s (%zu ranges)",first,last,m->range_count);
    add_event(m,MON_SCAN_STARTED,SIZE_MAX,NULL,track_absent ? detail : "Single host",now);
}
static size_t find_host(const monitor_t *m, const lan_host_t *v) {
    for (size_t i = 0; i < *m->count; ++i) {
        const lan_host_t *h = &m->hosts[i];
        if (h->interface_index == v->interface_index && h->address.family == v->address.family && h->address.scope_id == v->address.scope_id && !memcmp(h->address.bytes,v->address.bytes,16)) return i;
    }
    return SIZE_MAX;
}
size_t monitor_observe(monitor_t *m, const lan_host_t *incoming, bool confirmed, uint64_t now) {
    size_t index = find_host(m,incoming); bool existed = index != SIZE_MAX;
    bool known = existed && m->hosts[index].first_seen != 0;
    bool online = existed && m->hosts[index].online;
    bool mac_changed = confirmed && known && m->hosts[index].has_mac && incoming->has_mac && memcmp(m->hosts[index].mac,incoming->mac,6);
    bool name_changed = existed && m->hosts[index].hostname[0] && incoming->hostname[0] && strncmp(m->hosts[index].hostname,incoming->hostname,256);
    char old_mac[256] = "", new_mac[256] = "", old_name[256] = "";
    if (mac_changed) { mac_text(m->hosts[index].mac,old_mac); mac_text(incoming->mac,new_mac); }
    if (name_changed) text_copy(old_name,m->hosts[index].hostname);
    lan_host_t value = *incoming;
    /* A cached MAC or late DNS snapshot must never prove liveness or overwrite
       a confirmed MAC. DNS only supplies an enrichment update. */
    if (!confirmed) {
        value.online = false; value.has_latency = false;
        value.first_seen = value.last_seen = 0;
        value.ports_checked = false;
        if (known) value.has_mac = false;
    } else {
        value.online = true; value.cache_only = false;
        if (!value.first_seen) value.first_seen = now;
        if (!value.last_seen) value.last_seen = now;
    }
    index = host_merge(m->hosts,m->count,m->capacity,&value);
    if (index == SIZE_MAX) return index;
    lan_host_t *host = &m->hosts[index];
    if (confirmed) {
        if (!incoming->has_latency) host->has_latency = false;
        if (mac_changed) {
            if (!incoming->ports_checked) { host->port_count = 0; host->ports_checked = false; host->os_family = OS_UNKNOWN; host->os_confidence = 0; }
            if (!incoming->hostname[0]) host->hostname[0] = 0;
        }
        host->seen_round = m->round; host->missed_rounds = 0;
        if (!online) host->ip_correlated = false;
        if (!known) {
            host->new_device = m->mark_new;
            add_event(m,m->mark_new ? MON_NEW_HOST : MON_HOST_ONLINE,index,NULL,host->hostname,now);
        } else if (!online) add_event(m,MON_HOST_ONLINE,index,NULL,host->hostname,now);
        if (mac_changed) { host->changed = true; host->new_device = true; add_event(m,MON_MAC_CHANGED,index,old_mac,new_mac,now); add_event(m,MON_NEW_HOST,index,NULL,new_mac,now); }
    }
    if (name_changed) { host->changed = true; add_event(m,MON_HOSTNAME_CHANGED,index,old_name,host->hostname,now); }
    return index;
}
static void correlate_ip(monitor_t *m, size_t old_index, uint64_t now) {
    lan_host_t *old = &m->hosts[old_index];
    if (!old->has_mac || (old->mac[0]&3) || old->ip_correlated) return;
    size_t found = SIZE_MAX; unsigned candidates = 0;
    for (size_t i = 0; i < *m->count; ++i) {
        lan_host_t *current = &m->hosts[i];
        if (i != old_index && current->interface_index == old->interface_index && current->has_mac && !memcmp(old->mac,current->mac,6)) { ++candidates; if (current->online) found = i; }
    }
    if (candidates != 1 || found == SIZE_MAX) return; /* Avoid proxy-ARP / shared MAC guesses. */
    char previous[32], current[32]; ipv4_format(ipv4_value(&old->address),previous,32); ipv4_format(ipv4_value(&m->hosts[found].address),current,32);
    m->hosts[found].changed = true; old->ip_correlated = true;
    add_event(m,MON_IP_CHANGED,found,previous,current,now);
}
void monitor_finish(monitor_t *m, bool complete, uint64_t now) {
    if (complete && m->track_absent) {
        for (size_t i = 0; i < *m->count; ++i) {
            lan_host_t *h = &m->hosts[i]; uint32_t ip = ipv4_value(&h->address);
            if (h->address.family != 4 || h->interface_index != m->interface_index || !ranges_contains(m->ranges,m->range_count,ip) || !h->first_seen) continue;
            if (h->seen_round == m->round) { h->missed_rounds = 0; continue; }
            if (h->missed_rounds < 255) ++h->missed_rounds;
            if (h->online && h->missed_rounds >= m->offline_threshold) { h->online = false; h->has_latency = false; add_event(m,MON_HOST_OFFLINE,i,NULL,h->hostname,now); }
        }
        for (size_t i = 0; i < *m->count; ++i) if (!m->hosts[i].online && m->hosts[i].first_seen) correlate_ip(m,i,now);
    }
    if (complete) m->baseline = true;
    add_event(m,complete ? MON_SCAN_FINISHED : MON_SCAN_CANCELLED,SIZE_MAX,NULL,NULL,now);
}
void monitor_note(monitor_t *m, monitor_event_type_t type, const char *detail, uint64_t now) { add_event(m,type,SIZE_MAX,NULL,detail,now); }
const monitor_event_t *monitor_log_at(const monitor_t *m, size_t i) { return i < m->log_count ? &m->log[(m->log_head+i)%m->log_capacity] : NULL; }
void monitor_acknowledge(monitor_t *m, size_t i) { if (i < *m->count) { m->hosts[i].new_device = false; m->hosts[i].changed = false; } }
