#ifndef WIN_NBNS_H
#define WIN_NBNS_H
#include "win_resolver.h"
bool win_nbns_name(uint32_t source, uint32_t target, char name[256], uint32_t timeout_ms, resolver_cancel_t cancel, void *context);
#endif
