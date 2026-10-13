#ifndef POSIX_TOOLS_H
#define POSIX_TOOLS_H
#include "posix_net.h"
#include "details.h"
typedef struct posix_tools posix_tools_t;
posix_tools_t *posix_tools_start(const posix_interface_t *nic,uint32_t target,unsigned timeout,bool trace);
posix_tools_t *posix_forward_start(const posix_interface_t *nic,const char *name);
void posix_tools_cancel(posix_tools_t *job);
bool posix_tools_pop(posix_tools_t *job,ping_sample_t *sample);
bool posix_tools_reap(posix_tools_t *job);
void posix_tools_free(posix_tools_t *job);
scan_result_t posix_echo(posix_backend_t *backend,uint32_t target,unsigned timeout,lan_host_t *host);
#endif
