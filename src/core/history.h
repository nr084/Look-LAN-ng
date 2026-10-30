#ifndef LAN_HISTORY_H
#define LAN_HISTORY_H
#include "monitor.h"
#include "profile.h"
#define HISTORY_MAX_HOSTS 65536u
#define HISTORY_MAX_EVENTS 10000u
#define HISTORY_MAX_BYTES (176u + HISTORY_MAX_HOSTS*448u + HISTORY_MAX_EVENTS*552u + 4u)
/* Versioned, endian-independent snapshot. No OS handles or struct padding on disk. */
bool history_write(const monitor_t *model,const profile_t *profile,text_writer_t writer,void *context);
/* Transactional: malformed/truncated/foreign-interface input leaves model unchanged. */
bool history_read(const void *data,size_t size,monitor_t *model,const profile_t *profile,uint32_t interface_index);
#endif
