#include "details.h"
#include <string.h>
void details_reset(details_store_t *s) { s->ping_head = s->ping_count = s->note_count = 0; }
void details_ping_add(details_store_t *s, ping_sample_t sample) {
    if (s->ping_count == PING_HISTORY_CAPACITY) { s->ping_head = (s->ping_head+1)%PING_HISTORY_CAPACITY; --s->ping_count; }
    s->pings[(s->ping_head+s->ping_count++)%PING_HISTORY_CAPACITY] = sample;
}
const ping_sample_t *details_ping_at(const details_store_t *s, size_t i) { return i < s->ping_count ? &s->pings[(s->ping_head+i)%PING_HISTORY_CAPACITY] : NULL; }
const char *details_note(const details_store_t *s, uint32_t ip, uint32_t interface_index) {
    for (size_t i = 0; i < s->note_count; ++i) if (s->notes[i].ip == ip && s->notes[i].interface_index == interface_index) return s->notes[i].text;
    return "";
}
bool details_note_set(details_store_t *s, uint32_t ip, uint32_t interface_index, const char *text) {
    if (!text || strlen(text) >= sizeof(s->notes[0].text)) return false;
    size_t i; for (i = 0; i < s->note_count; ++i) if (s->notes[i].ip == ip && s->notes[i].interface_index == interface_index) break;
    if (!text[0]) { if (i < s->note_count) s->notes[i] = s->notes[--s->note_count]; return true; }
    if (i == s->note_count) { if (i == DEVICE_NOTES_CAPACITY) return false; ++s->note_count; }
    s->notes[i].ip = ip; s->notes[i].interface_index = interface_index; memcpy(s->notes[i].text,text,strlen(text)+1); return true;
}
