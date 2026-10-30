#ifndef HISTORY_STORE_H
#define HISTORY_STORE_H
#include "history.h"
typedef struct history_store history_store_t;
/* Paths are UTF-8. Only immutable snapshots are touched by the worker. */
history_store_t *history_store_start(const char *path,const profile_t *profile,const monitor_t *model,bool load,uint32_t interface_index);
bool history_store_done(history_store_t *job);
bool history_store_success(const history_store_t *job);
bool history_store_exists(const char *path);
void history_store_apply(const history_store_t *job,monitor_t *model);
void history_store_free(history_store_t *job);
#endif
