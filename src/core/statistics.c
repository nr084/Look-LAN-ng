#include "statistics.h"
statistics_t statistics_compute(const lan_host_t *hosts, size_t count, uint32_t scanned) {
    statistics_t s = {0}; s.records = count; s.scanned = scanned;
    for (size_t i = 0; i < count; ++i) {
        const lan_host_t *h = &hosts[i];
        if (h->new_device) ++s.fresh;
        if (!h->first_seen) ++s.unknown; else if (!h->online) ++s.offline;
        if (!h->online) continue;
        ++s.online;
        if (h->os_family == OS_WINDOWS) ++s.windows;
        else if (h->os_family == OS_LINUX || h->os_family == OS_MACOS || h->os_family == OS_BSD) ++s.unix_hosts;
        else if (h->os_family == OS_NETWORK_DEVICE || h->os_family == OS_EMBEDDED || h->os_family == OS_PRINTER || h->os_family == OS_NAS) ++s.network_devices;
        else ++s.unknown_os;
    }
    return s;
}
