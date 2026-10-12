#ifndef STATISTICS_H
#define STATISTICS_H
#include "host.h"
typedef struct { size_t records, online, offline, unknown, fresh, windows, unix_hosts, network_devices, unknown_os; uint32_t scanned; } statistics_t;
statistics_t statistics_compute(const lan_host_t *hosts, size_t count, uint32_t scanned);
#endif
