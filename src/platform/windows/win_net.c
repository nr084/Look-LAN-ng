#include "win_net.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wctype.h>
#include "ports.h"
#include "win_resolver.h"
#include "win_nbns.h"

static uint64_t wall_time(void *ctx) { (void)ctx; return (uint64_t)time(NULL); }
static bool cancelled(void *ctx) { return InterlockedCompareExchange(&((win_backend_t *)ctx)->cancel, 0, 0) != 0; }
static bool pace_probe(win_backend_t *b) {
    EnterCriticalSection(&b->lock);
    ULONGLONG now = GetTickCount64(), ticket = b->next_probe > now ? b->next_probe : now;
    b->next_probe = ticket+(b->monitoring ? 16 : 4);
    LeaveCriticalSection(&b->lock);
    while (!cancelled(b)) { now = GetTickCount64(); if (now >= ticket) return true; Sleep((DWORD)(ticket-now > 25 ? 25 : ticket-now)); }
    return false;
}
typedef struct { win_backend_t *backend; LONG generation; } resolver_context_t;
static bool resolver_stopped(void *context) {
    resolver_context_t *query = context;
    return cancelled(query->backend) || query->generation != InterlockedCompareExchange(&query->backend->generation,0,0) || InterlockedCompareExchange(&query->backend->shutdown,0,0);
}

DWORD win_enumerate(win_interface_t *out, size_t cap, size_t *count) {
    ULONG size = 16384;
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    DWORD result = ERROR_NOT_ENOUGH_MEMORY;
    *count = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        adapters = malloc(size);
        if (!adapters) return ERROR_NOT_ENOUGH_MEMORY;
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, NULL, adapters, &size);
        if (result != ERROR_BUFFER_OVERFLOW) break;
        free(adapters); adapters = NULL;
    }
    if (result != NO_ERROR) { free(adapters); return result; }
    for (IP_ADAPTER_ADDRESSES *a = adapters; a && *count < cap; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u && *count < cap; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET || u->OnLinkPrefixLength > 32) continue;
            win_interface_t v = {0};
            v.ip = ntohl(((SOCKADDR_IN *)u->Address.lpSockaddr)->sin_addr.s_addr);
            v.prefix = u->OnLinkPrefixLength; v.index = a->IfIndex; v.metric = a->Ipv4Metric;
            if (!v.ip) continue;
            swprintf_s(v.name, 128, L"%ls", a->FriendlyName ? a->FriendlyName : L"Network");
            snprintf(v.id,sizeof(v.id),"%s",a->AdapterName ? a->AdapterName : "");
            swprintf_s(v.description, 256, L"%ls", a->Description ? a->Description : L"");
            if (a->PhysicalAddressLength == 6) { memcpy(v.mac, a->PhysicalAddress, 6); v.has_mac = true; }
            if (a->FirstGatewayAddress && a->FirstGatewayAddress->Address.lpSockaddr->sa_family == AF_INET)
                v.gateway = ntohl(((SOCKADDR_IN *)a->FirstGatewayAddress->Address.lpSockaddr)->sin_addr.s_addr);
            size_t used = 0;
            for (IP_ADAPTER_DNS_SERVER_ADDRESS *d = a->FirstDnsServerAddress; d; d = d->Next) {
                if (d->Address.lpSockaddr->sa_family != AF_INET) continue;
                wchar_t text[48];
                InetNtopW(AF_INET, &((SOCKADDR_IN *)d->Address.lpSockaddr)->sin_addr, text, 48);
                int n = swprintf_s(v.dns + used, 256 - used, L"%ls%ls", used ? L", " : L"", text);
                if (n < 0) break;
                used += (size_t)n;
                if (used > 200) break;
            }
            MIB_IF_ROW2 row = {0}; row.InterfaceIndex = a->IfIndex;
            bool physical = GetIfEntry2(&row) == NO_ERROR && row.InterfaceAndOperStatusFlags.HardwareInterface;
            wchar_t lower[256];
            for (size_t i = 0; i < 256; ++i) lower[i] = (wchar_t)towlower(v.description[i]);
            bool virtual_adapter = wcsstr(lower, L"virtual") || wcsstr(lower, L"vpn") || wcsstr(lower, L"hyper-v") || wcsstr(lower, L"tunnel") || wcsstr(lower, L"tap-");
            v.automatic = physical && !virtual_adapter && (a->IfType == IF_TYPE_ETHERNET_CSMACD || a->IfType == IF_TYPE_IEEE80211) && (v.ip >> 16) != 0xa9fe;
            out[(*count)++] = v;
        }
    }
    free(adapters);
    return NO_ERROR;
}

/* Bounded queue: producers back off off the UI thread; no per-host window messages. */
static void emit(win_backend_t *b, const net_event_t *event) {
    for (;;) {
        EnterCriticalSection(&b->lock);
        if (b->count < EVENT_CAPACITY) {
            b->events[(b->head + b->count++) % EVENT_CAPACITY] = *event;
            LeaveCriticalSection(&b->lock); return;
        }
        LeaveCriticalSection(&b->lock);
        if (InterlockedCompareExchange(&b->shutdown,0,0) || (cancelled(b) && event->type != NET_DONE)) return;
        Sleep(5);
    }
}
bool win_event_pop(win_backend_t *b, net_event_t *out) {
    EnterCriticalSection(&b->lock);
    bool available = b->count != 0;
    if (available) { *out = b->events[b->head]; b->head = (b->head + 1) % EVENT_CAPACITY; --b->count; }
    LeaveCriticalSection(&b->lock); return available;
}
static void enqueue_dns(win_backend_t *b, const lan_host_t *host, LONG generation) {
    EnterCriticalSection(&b->lock);
    if (b->dns_count < DNS_CAPACITY) {
        dns_task_t *task = &b->dns_tasks[(b->dns_head + b->dns_count++) % DNS_CAPACITY];
        task->host = *host; task->generation = generation; task->source = b->selected.ip;
        SetEvent(b->wake);
    }
    LeaveCriticalSection(&b->lock);
}
static DWORD WINAPI dns_worker(void *parameter) {
    win_backend_t *b = parameter;
    for (;;) {
        WaitForSingleObject(b->wake, INFINITE);
        if (InterlockedCompareExchange(&b->shutdown,0,0)) return 0;
        dns_task_t task;
        EnterCriticalSection(&b->lock);
        if (InterlockedCompareExchange(&b->shutdown,0,0)) { LeaveCriticalSection(&b->lock); return 0; }
        if (!b->dns_count) { ResetEvent(b->wake); LeaveCriticalSection(&b->lock); continue; }
        task = b->dns_tasks[b->dns_head]; b->dns_head = (b->dns_head + 1) % DNS_CAPACITY; --b->dns_count;
        InterlockedIncrement(&b->dns_active);
        LeaveCriticalSection(&b->lock);
        if (task.generation != InterlockedCompareExchange(&b->generation, 0, 0) || cancelled(b)) { InterlockedDecrement(&b->dns_active); continue; }
        wchar_t name[256];
        resolver_context_t query = {b,task.generation};
        net_event_t event = {0}; event.type = NET_DNS; event.generation = task.generation; event.host = task.host;
        bool found = win_resolve_name(ipv4_value(&task.host.address),task.host.interface_index,name,256,3000,resolver_stopped,&query) == ERROR_SUCCESS && WideCharToMultiByte(CP_UTF8,0,name,-1,event.host.hostname,256,NULL,NULL);
        if (!found && !resolver_stopped(&query)) found = win_nbns_name(task.source,ipv4_value(&task.host.address),event.host.hostname,300,resolver_stopped,&query);
        if (found && !resolver_stopped(&query)) emit(b,&event);
        InterlockedDecrement(&b->dns_active);
    }
}
static scan_result_t arp_probe(void *ctx, uint32_t ip, lan_host_t *h) {
    win_backend_t *b = ctx;
    h->interface_index = b->selected.index;
    if (ip == b->selected.ip) { if (b->selected.has_mac) { memcpy(h->mac, b->selected.mac, 6); h->has_mac = true; } return SCAN_OK; }
    uint32_t mask = b->selected.prefix ? UINT32_MAX << (32-b->selected.prefix) : 0;
    if ((ip & mask) != (b->selected.ip & mask)) return SCAN_NO_ROUTE;
    if (!pace_probe(b)) return SCAN_CANCELLED;
    scan_result_t result = win_neighbor_probe(&b->neighbor,b->selected.ip,ip,b->selected.index,h->mac,cancelled,b);
    if (result == SCAN_OK) h->has_mac = true;
    return result;
}
static scan_result_t ping_probe(void *ctx, uint32_t ip, uint32_t timeout, lan_host_t *h) {
    win_backend_t *b = ctx;
    if (!pace_probe(b)) return SCAN_CANCELLED;
    win_echo_result_t echo; scan_result_t result = win_ping_probe_ttl(&b->ping,b->selected.ip,ip,timeout,0,h,&echo,cancelled,b);
    if (result != SCAN_CANCELLED) { net_event_t event = {0}; event.type = NET_PING; event.generation = b->generation; event.host = *h; event.error = echo.status; emit(b,&event); }
    return result;
}
static scan_result_t tcp_probe(void *ctx, uint32_t ip, uint16_t port, uint32_t timeout) {
    win_backend_t *b = ctx;
    timeout = b->tcp_timeout_ms;
    if (cancelled(b)) return SCAN_CANCELLED;
    if (b->monitoring) {
        size_t low = 0, high = b->known_count;
        while (low < high) { size_t middle = low+(high-low)/2; if (b->known_ips[middle] < ip) low = middle+1; else high = middle; }
        if (low == b->known_count || b->known_ips[low] != ip) return SCAN_UNREACHABLE;
    }
    if (!pace_probe(b)) return SCAN_CANCELLED;
    SOCKET socket_handle = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) return SCAN_SYSTEM_ERROR;
    SOCKADDR_IN local = {0}; local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(b->selected.ip);
    if (bind(socket_handle,(SOCKADDR *)&local,sizeof(local)) == SOCKET_ERROR) { closesocket(socket_handle); return SCAN_SYSTEM_ERROR; }
    u_long nonblocking = 1;
    if (ioctlsocket(socket_handle,FIONBIO,&nonblocking) == SOCKET_ERROR) { closesocket(socket_handle); return SCAN_SYSTEM_ERROR; }
    SOCKADDR_IN address = {0}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(ip); address.sin_port = htons(port);
    int result = connect(socket_handle,(SOCKADDR *)&address,sizeof(address));
    int error = result ? WSAGetLastError() : 0;
    if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS) {
        ULONGLONG end = GetTickCount64()+timeout;
        error = WSAETIMEDOUT;
        while (!cancelled(b) && GetTickCount64() < end) {
            fd_set write_set, error_set; FD_ZERO(&write_set); FD_ZERO(&error_set); FD_SET(socket_handle,&write_set); FD_SET(socket_handle,&error_set);
            ULONGLONG remaining = end-GetTickCount64();
            TIMEVAL wait = {0,(long)((remaining < 50 ? remaining : 50)*1000)};
            int ready = select(0,NULL,&write_set,&error_set,&wait);
            if (ready == SOCKET_ERROR) { error = WSAGetLastError(); break; }
            if (ready) { int length = sizeof(error); if (getsockopt(socket_handle,SOL_SOCKET,SO_ERROR,(char *)&error,&length)) error = WSAGetLastError(); break; }
        }
    }
    closesocket(socket_handle);
    if (cancelled(b)) return SCAN_CANCELLED;
    if (!error) return SCAN_OK;
    if (error == WSAECONNREFUSED) return SCAN_REFUSED;
    if (error == WSAETIMEDOUT) return SCAN_TIMEOUT;
    if (error == WSAENETUNREACH || error == WSAEHOSTUNREACH) return SCAN_NO_ROUTE;
    return SCAN_SYSTEM_ERROR;
}
static const platform_net_ops_t ops = {arp_probe, ping_probe, cancelled, wall_time, tcp_probe};
static DWORD WINAPI probe_worker(void *parameter) {
    win_backend_t *b = parameter;
    while (!cancelled(b)) {
        LONG offset = InterlockedIncrement(&b->next) - 1;
        if ((uint32_t)offset >= b->total) break;
        uint32_t ip; if (!ranges_address_at(b->ranges,b->range_count,(uint32_t)offset,&ip)) break;
        lan_host_t host;
        if (scanner_probe(&ops, b, ip, b->icmp_timeout_ms, &host) == SCAN_OK) {
            net_event_t e = {0}; e.type = NET_HOST; e.generation = b->generation; e.host = host;
            emit(b, &e); enqueue_dns(b, &host, e.generation);
            if (!b->monitoring && scanner_services(&ops,b,&host,b->service_ports,b->service_count,b->tcp_timeout_ms) == SCAN_OK) { e.host = host; emit(b,&e); }
        }
        InterlockedIncrement(&b->completed);
    }
    return 0;
}
static void seed_cache(win_backend_t *b) {
    MIB_IPNET_TABLE2 *table = NULL;
    if (GetIpNetTable2(AF_INET, &table) != NO_ERROR) return;
    for (ULONG i = 0; i < table->NumEntries && !cancelled(b); ++i) {
        MIB_IPNET_ROW2 *row = &table->Table[i];
        uint32_t ip = ntohl(row->Address.Ipv4.sin_addr.s_addr);
        if (row->InterfaceIndex != b->selected.index || !ranges_contains(b->ranges,b->range_count,ip) || row->PhysicalAddressLength != 6 || row->State == NlnsUnreachable || row->State == NlnsIncomplete) continue;
        net_event_t e = {0}; e.type = NET_HOST; e.generation = b->generation;
        e.host.address = ipv4_address(ip); e.host.has_mac = true; e.host.cache_only = true; e.host.interface_index = row->InterfaceIndex;
        memcpy(e.host.mac, row->PhysicalAddress, 6); emit(b, &e);
    }
    FreeMibTable(table);
}
static DWORD WINAPI coordinate(void *parameter) {
    win_backend_t *b = parameter;
    if (b->monitoring) {
        MIB_IF_ROW2 interface = {0}; interface.InterfaceIndex = b->selected.index;
        MIB_UNICASTIPADDRESS_ROW address = {0}; address.InterfaceIndex = b->selected.index;
        address.Address.Ipv4.sin_family = AF_INET; address.Address.Ipv4.sin_addr.s_addr = htonl(b->selected.ip);
        if (GetIfEntry2(&interface) != NO_ERROR || interface.OperStatus != IfOperStatusUp || GetUnicastIpAddressEntry(&address) != NO_ERROR) {
            net_event_t e = {0}; e.generation = b->generation; e.type = NET_ERROR; e.error = ERROR_NETWORK_UNREACHABLE; emit(b,&e); e.type = NET_DONE; emit(b,&e); return 0;
        }
    }
    if (b->target[0] && !cancelled(b)) {
        uint32_t ip;
        resolver_context_t query = {b,b->generation};
        DWORD error = win_resolve_ipv4(b->target,&ip,3000,resolver_stopped,&query);
        if (!error) {
            b->range.first = b->range.last = ip;
            b->ranges[0] = b->range;
        } else {
            net_event_t e = {0}; e.type = NET_ERROR; e.generation = b->generation; e.error = error;
            if (!cancelled(b) && error != ERROR_CANCELLED) emit(b,&e);
            e.type = NET_DONE; e.cancelled = cancelled(b); emit(b,&e); return 0;
        }
    }
    seed_cache(b);
    HANDLE workers[256]; DWORD n = 0;
    unsigned concurrency = b->monitoring ? (b->concurrency < 16 ? b->concurrency : 16) : b->concurrency;
    unsigned limit = b->total < concurrency ? b->total : concurrency;
    for (unsigned i = 0; i < limit && !cancelled(b); ++i) {
        HANDLE thread = CreateThread(NULL, 0, probe_worker, b, 0, NULL);
        if (thread) workers[n++] = thread;
    }
    if (n) { for (DWORD i = 0; i < n; ++i) { WaitForSingleObject(workers[i],INFINITE); CloseHandle(workers[i]); } }
    else if (!cancelled(b)) { net_event_t e = {0}; e.type = NET_ERROR; e.generation = b->generation; e.error = ERROR_NOT_ENOUGH_MEMORY; emit(b, &e); }
    net_event_t done = {0}; done.type = NET_DONE; done.generation = b->generation;
    done.completed = (uint32_t)b->completed; done.cancelled = cancelled(b); emit(b, &done);
    return 0;
}
bool win_backend_init(win_backend_t *b) {
    if (b->initialized) return false;
    InterlockedExchange(&b->shutdown,0); InterlockedExchange(&b->cancel,0);
    b->head = b->count = b->dns_head = b->dns_count = 0;
    b->dns_active = 0; b->next_probe = 0;
    InitializeCriticalSection(&b->lock);
    b->wake = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!b->wake) { DeleteCriticalSection(&b->lock); return false; }
    if (!win_ping_init(&b->ping)) { CloseHandle(b->wake); b->wake = NULL; DeleteCriticalSection(&b->lock); return false; }
    b->initialized = true;
    if (!win_neighbor_init(&b->neighbor)) { win_backend_shutdown(b); while (!win_backend_release(b)) Sleep(1); return false; }
    for (unsigned i = 0; i < 4; ++i) {
        b->dns_threads[i] = CreateThread(NULL, 0, dns_worker, b, 0, NULL);
        if (!b->dns_threads[i]) { win_backend_shutdown(b); while (!win_backend_release(b)) Sleep(1); return false; }
    }
    return true;
}
void win_backend_shutdown(win_backend_t *b) {
    if (!b->initialized) return;
    InterlockedExchange(&b->shutdown,1); win_scan_stop(b); win_ping_stop(&b->ping); win_neighbor_stop(&b->neighbor); SetEvent(b->wake);
}
bool win_backend_release(win_backend_t *b) {
    if (!b->initialized) return true;
    if (!InterlockedCompareExchange(&b->shutdown,0,0) || win_scan_running(b)) return false;
    for (unsigned i = 0; i < 4; ++i) if (b->dns_threads[i] && WaitForSingleObject(b->dns_threads[i],0) != WAIT_OBJECT_0) return false;
    if (!win_ping_release(&b->ping)) return false;
    if (!win_neighbor_release(&b->neighbor)) return false;
    if (b->coordinator) { CloseHandle(b->coordinator); b->coordinator = NULL; }
    for (unsigned i = 0; i < 4; ++i) if (b->dns_threads[i]) { CloseHandle(b->dns_threads[i]); b->dns_threads[i] = NULL; }
    CloseHandle(b->wake); b->wake = NULL; DeleteCriticalSection(&b->lock); b->initialized = false;
    return true;
}
bool win_scan_running(win_backend_t *b) { return b->coordinator && WaitForSingleObject(b->coordinator, 0) == WAIT_TIMEOUT; }
static int compare_ip(const void *a, const void *c) { uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)c; return (x > y)-(x < y); }
static bool start(win_backend_t *b, const win_interface_t *v, const ipv4_range_t *ranges, size_t range_count, const wchar_t *target, const uint16_t *ports, size_t port_count, bool monitoring, const lan_host_t *hosts, size_t host_count, const profile_t *settings) {
    if (!b->initialized || InterlockedCompareExchange(&b->shutdown,0,0) || win_scan_running(b) || !ranges || !range_count || range_count > NETWORK_MAX_RANGES) return false;
    ipv4_range_t normalized[NETWORK_MAX_RANGES]; memcpy(normalized,ranges,range_count*sizeof(*ranges)); uint64_t total;
    if (!ranges_normalize(normalized,&range_count,&total) || total > INT32_MAX) return false;
    if (!ports || !port_count || port_count > 65535 || (target && wcslen(target) >= 256)) return false;
    if (b->coordinator) { CloseHandle(b->coordinator); b->coordinator = NULL; }
    EnterCriticalSection(&b->lock);
    b->dns_count = 0; b->dns_head = 0;
    LeaveCriticalSection(&b->lock);
    InterlockedIncrement(&b->generation); InterlockedExchange(&b->cancel, 0);
    InterlockedExchange(&b->next, 0); InterlockedExchange(&b->completed, 0);
    b->next_probe = 0;
    b->selected = *v; b->range = normalized[0]; memcpy(b->ranges,normalized,range_count*sizeof(*ranges)); b->range_count = range_count; b->total = (uint32_t)total;
    b->icmp_timeout_ms = settings ? settings->icmp_timeout_ms : 300; b->tcp_timeout_ms = settings ? settings->tcp_timeout_ms : 250; b->concurrency = settings ? settings->concurrency : 64;
    if (b->concurrency < 8 || b->concurrency > 256) return false;
    b->target[0] = 0; if (target) wcscpy_s(b->target,256,target);
    memcpy(b->service_ports,ports,port_count*sizeof(*ports)); b->service_count = port_count;
    b->monitoring = monitoring; b->known_count = 0;
    if (monitoring) {
        for (size_t i = 0; i < host_count && b->known_count < 65536; ++i) if (hosts[i].address.family == 4 && hosts[i].first_seen && hosts[i].interface_index == v->index) b->known_ips[b->known_count++] = ipv4_value(&hosts[i].address);
        qsort(b->known_ips,b->known_count,sizeof(*b->known_ips),compare_ip);
    }
    b->coordinator = CreateThread(NULL, 0, coordinate, b, 0, NULL);
    return b->coordinator != NULL;
}
bool win_scan_start(win_backend_t *b, const win_interface_t *v, ipv4_range_t range) { return start(b,v,&range,1,NULL,discovery_ports,discovery_port_count,false,NULL,0,NULL); }
bool win_scan_host_start(win_backend_t *b, const win_interface_t *v, const wchar_t *target, const uint16_t *ports, size_t count) { ipv4_range_t range = {0,0}; return start(b,v,&range,1,target,ports,count,false,NULL,0,NULL); }
bool win_monitor_start(win_backend_t *b, const win_interface_t *v, ipv4_range_t range, const lan_host_t *hosts, size_t count) { return start(b,v,&range,1,NULL,discovery_ports,discovery_port_count,true,hosts,count,NULL); }
bool win_profile_scan_start(win_backend_t *b, const win_interface_t *v, const profile_t *p, bool monitoring, const lan_host_t *hosts, size_t count) { return profile_validate(p,NULL,0) && start(b,v,p->ranges,p->range_count,NULL,p->ports,p->port_count,monitoring,hosts,count,p); }
bool win_profile_host_start(win_backend_t *b, const win_interface_t *v, const wchar_t *target, const uint16_t *ports, size_t count, const profile_t *p) { ipv4_range_t range = {0,0}; return start(b,v,&range,1,target,ports,count,false,NULL,0,p); }
void win_scan_stop(win_backend_t *b) { InterlockedExchange(&b->cancel, 1); }
