#ifndef WIN_TOOLS_H
#define WIN_TOOLS_H
#include "win_ping.h"
#include "details.h"
typedef struct win_tools_job win_tools_job_t;
win_tools_job_t *win_tools_start(win_ping_pool_t *pool, uint32_t source, uint32_t target, uint32_t interface_index, uint32_t timeout, bool trace);
void win_tools_cancel(win_tools_job_t *job);
bool win_tools_pop(win_tools_job_t *job, ping_sample_t *sample);
bool win_tools_finished(win_tools_job_t *job);
void win_tools_free(win_tools_job_t *job);
#endif
