#include "win_net.h"
#include <stdio.h>
#include "win_resolver.h"
#include "win_tools.h"
static win_backend_t backend;
static SOCKET repeat_listener = INVALID_SOCKET;
static uint16_t repeat_port;
static profile_t scan_profile;
static int test_diagnostics(const win_interface_t *interface) {
    for (unsigned trace = 0; trace < 2; ++trace) {
        win_tools_job_t *job = win_tools_start(&backend.ping,interface->ip,interface->ip,interface->index,300,trace != 0); if (!job) return 1;
        ULONGLONG deadline = GetTickCount64()+12000;
        while (!win_tools_finished(job)) { if (GetTickCount64() > deadline) { win_tools_cancel(job); return 2; } Sleep(1); }
        ping_sample_t sample; unsigned count = 0; bool reached = false;
        while (win_tools_pop(job,&sample)) {
            ++count;
            if (sample.ip != interface->ip || sample.interface_index != interface->index) { win_tools_free(job); return 3; }
            if (sample.received && !sample.status && sample.reply_ip == interface->ip) reached = true;
        }
        win_tools_free(job);
        if (!reached || (!trace && count != 4) || (trace && count != 1)) return 4;
    }
    puts("Diagnostics integration passed: four source-bound own-IP ping replies and one-hop native traceroute."); return 0;
}
static bool quiescent(void) {
    ULONGLONG deadline = GetTickCount64()+5000;
    for (;;) {
        net_event_t event; while (win_event_pop(&backend,&event)) { }
        EnterCriticalSection(&backend.lock); bool dns_idle = !backend.dns_count && !backend.dns_active; LeaveCriticalSection(&backend.lock);
        EnterCriticalSection(&backend.ping.lock); bool ping_idle = !backend.ping.count; LeaveCriticalSection(&backend.ping.lock);
        if (dns_idle && ping_idle && !win_scan_running(&backend)) return true;
        if (GetTickCount64() > deadline) return false;
        Sleep(1);
    }
}
static int test_cancel_hostname(const win_interface_t *interface) {
    for (unsigned round = 0; round < 20; ++round) {
        if (!win_scan_host_start(&backend,interface,L"localhost",&repeat_port,1)) return 1;
        win_scan_stop(&backend); bool done = false;
        ULONGLONG deadline = GetTickCount64()+2000;
        do {
            net_event_t event;
            while (win_event_pop(&backend,&event)) {
                if (event.generation != backend.generation) continue;
                if (event.type == NET_ERROR) return 2;
                if (event.type == NET_DONE) { if (!event.cancelled) return 3; done = true; }
            }
            if (done && !win_scan_running(&backend)) break;
            Sleep(1);
        } while (GetTickCount64() < deadline);
        if (!done || win_scan_running(&backend)) return 4;
    }
    puts("Hostname cancellation passed: 20 cancelled scans without NET_ERROR."); return 0;
}
static int test_profile(const win_interface_t *interface) {
    profile_defaults(&scan_profile); scan_profile.ranges[0] = scan_profile.ranges[1] = (ipv4_range_t){interface->ip,interface->ip}; scan_profile.range_count = 2;
    scan_profile.ports[0] = repeat_port; scan_profile.port_count = 1; scan_profile.icmp_timeout_ms = 50; scan_profile.tcp_timeout_ms = 25; scan_profile.concurrency = 256;
    win_interface_t selected = *interface; selected.has_mac = false;
    if (!win_profile_scan_start(&backend,&selected,&scan_profile,false,NULL,0)) return 1;
    ULONGLONG deadline = GetTickCount64()+10000; bool found = false, done = false;
    while (GetTickCount64() < deadline) {
        SOCKET peer = accept(repeat_listener,NULL,NULL); if (peer != INVALID_SOCKET) closesocket(peer);
        net_event_t e;
        while (win_event_pop(&backend,&e)) {
            if (e.generation != backend.generation) continue;
            if (e.type == NET_HOST && e.host.ports_checked && e.host.port_count == 1 && e.host.open_ports[0] == repeat_port) found = true;
            if (e.type == NET_HOST && e.host.online && e.host.has_mac) return 3;
            if (e.type == NET_DONE) done = true;
        }
        if (done && !win_scan_running(&backend)) break; Sleep(1);
    }
    if (!found || !done || backend.total != 1 || backend.range_count != 1 || backend.concurrency != 256 || backend.tcp_timeout_ms != 25 || backend.icmp_timeout_ms != 50) return 2;
    puts("Profile backend passed: duplicate own-IP ranges normalized, custom port and configured timeouts/concurrency applied."); return 0;
}
static int test_monitor(const win_interface_t *interface) {
    host_info_t known = {0}; known.address = ipv4_address(interface->ip); known.interface_index = interface->index; known.first_seen = 1;
    ipv4_range_t range = {interface->ip,interface->ip};
    if (!win_monitor_start(&backend,interface,range,&known,1)) return 1;
    ULONGLONG deadline = GetTickCount64()+10000; bool found = false, done = false, service_probe = false;
    while (GetTickCount64() < deadline) {
        net_event_t e;
        while (win_event_pop(&backend,&e)) {
            if (e.generation != backend.generation) continue;
            if (e.type == NET_HOST && e.host.online) found = true;
            if (e.type == NET_HOST && e.host.ports_checked) service_probe = true;
            if (e.type == NET_ERROR) return 2;
            if (e.type == NET_DONE) done = true;
        }
        if (done && !win_scan_running(&backend)) break;
        Sleep(1);
    }
    if (!found || !done || service_probe || !backend.monitoring) return 3;
    puts("Monitor backend passed: interface validation, own-host liveness, discovery-only round (no service rescan)."); return 0;
}
static int test_local_port(const win_interface_t *interface) {
    SOCKET listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return 1;
    SOCKADDR_IN address = {0}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(interface->ip);
    if (bind(listener,(SOCKADDR *)&address,sizeof(address)) || listen(listener,4)) { closesocket(listener); return 2; }
    int length = sizeof(address); getsockname(listener,(SOCKADDR *)&address,&length); uint16_t port = ntohs(address.sin_port);
    wchar_t target[32]; swprintf_s(target,32,L"%u.%u.%u.%u",interface->ip>>24,(interface->ip>>16)&255,(interface->ip>>8)&255,interface->ip&255);
    if (!win_scan_host_start(&backend,interface,target,&port,1)) { closesocket(listener); return 3; }
    ULONGLONG deadline = GetTickCount64()+10000; bool found = false, done = false;
    while (GetTickCount64() < deadline) {
        net_event_t e;
        while (win_event_pop(&backend,&e)) {
            if (e.generation != backend.generation) continue;
            if (e.type == NET_HOST && e.host.ports_checked && e.host.port_count == 1 && e.host.open_ports[0] == port) found = true;
            if (e.type == NET_DONE) done = true;
        }
        if (done && !win_scan_running(&backend)) break;
        Sleep(1);
    }
    closesocket(listener);
    if (!found || !done) return 4;
    puts("TCP integration passed: private local listener detected by single-host custom-port scan."); return 0;
}
static int run(const win_interface_t *interface, bool stop) {
    wchar_t target[32]; swprintf_s(target,32,L"%u.%u.%u.%u",interface->ip>>24,(interface->ip>>16)&255,(interface->ip>>8)&255,interface->ip&255);
    if (!win_scan_host_start(&backend,interface,target,&repeat_port,1)) return 1;
    if (stop) win_scan_stop(&backend);
    ULONGLONG deadline = GetTickCount64() + 10000;
    bool done = false, found = false;
    while (GetTickCount64() < deadline) {
        SOCKET peer = accept(repeat_listener,NULL,NULL); if (peer != INVALID_SOCKET) closesocket(peer);
        net_event_t e;
        while (win_event_pop(&backend,&e)) {
            if (e.generation != backend.generation) continue;
            if (e.type == NET_HOST && e.host.online && ipv4_value(&e.host.address) == interface->ip && e.host.interface_index == interface->index) found = true;
            if (e.type == NET_DONE) done = true;
        }
        if (done && !win_scan_running(&backend)) return stop || found ? 0 : 2;
        Sleep(1);
    }
    return 3;
}
int main(int argc, char **argv) {
    (void)argv;
    WSADATA data;
    if (WSAStartup(MAKEWORD(2,2),&data)) return 1;
    win_interface_t interfaces[MAX_INTERFACES]; size_t count;
    if (win_enumerate(interfaces,MAX_INTERFACES,&count)) return 2;
    if (!count) { puts("SKIP: no active IPv4 interface"); return 77; }
    size_t selected = 0;
    for (size_t i = 0; i < count; ++i) if (interfaces[i].automatic) { selected = i; break; }
    if (!win_backend_init(&backend)) return 3;
    if (argc > 1) return test_local_port(&interfaces[selected]);
    repeat_listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (repeat_listener == INVALID_SOCKET) return 8;
    SOCKADDR_IN address = {0}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(interfaces[selected].ip);
    if (bind(repeat_listener,(SOCKADDR *)&address,sizeof(address)) || listen(repeat_listener,16)) return 9;
    int address_size = sizeof(address);
    if (getsockname(repeat_listener,(SOCKADDR *)&address,&address_size)) return 10;
    repeat_port = ntohs(address.sin_port); u_long nonblocking = 1;
    if (ioctlsocket(repeat_listener,FIONBIO,&nonblocking)) return 11;
    /* Warm up all resolver workers and Winsock's lazily initialized resources
       before comparing handles across subsequent scan cycles. */
    for (unsigned i = 0; i < 500; ++i) if (run(&interfaces[selected],i % 5 == 0)) return 4;
    if (!quiescent()) return 24;
    DWORD before, after; GetProcessHandleCount(GetCurrentProcess(),&before);
    for (unsigned batch = 0; batch < 5; ++batch) {
        for (unsigned i = 0; i < 100; ++i) if (run(&interfaces[selected],i % 5 == 0)) return 5;
        if (!quiescent()) return 24;
        DWORD sample; GetProcessHandleCount(GetCurrentProcess(),&sample); printf("Handle sample after %u scans: %lu\n",(batch+1)*100,sample);
    }
    if (!quiescent()) return 24;
    GetProcessHandleCount(GetCurrentProcess(),&after);
    if (after > before) { fprintf(stderr,"Handle growth: %lu -> %lu\n",before,after); return 6; }
    printf("Backend passed: self-host discovery, interface binding, 500 repeated/cancelled scans with quiescent sampling. Handles: %lu -> %lu.\n",before,after);
    int tcp_test = test_local_port(&interfaces[selected]);
    if (tcp_test) { fprintf(stderr,"TCP integration failure: stage %d, Winsock error %d\n",tcp_test,WSAGetLastError()); return 7; }
    int monitor_test = test_monitor(&interfaces[selected]);
    if (monitor_test) { fprintf(stderr,"Monitor backend failure: stage %d\n",monitor_test); return 12; }
    int profile_test = test_profile(&interfaces[selected]);
    if (profile_test) { fprintf(stderr,"Profile backend failure: stage %d\n",profile_test); return 13; }
    if (test_cancel_hostname(&interfaces[selected])) return 22;
    if (test_diagnostics(&interfaces[selected])) return 25;
    closesocket(repeat_listener);
    uint32_t localhost = 0;
    if (win_resolve_ipv4(L"localhost",&localhost,3000,NULL,NULL) || (localhost >> 24) != 127) return 14;
    ULONGLONG shutdown_started = GetTickCount64();
    win_backend_shutdown(&backend);
    while (!win_backend_release(&backend)) {
        if (GetTickCount64()-shutdown_started > 2000) return 15;
        Sleep(1);
    }
    if (backend.initialized || backend.wake || backend.coordinator) return 16;
    for (unsigned i = 0; i < 4; ++i) if (backend.dns_threads[i]) return 17;
    printf("Resolver/shutdown passed: localhost, all backend workers joined and handles released (%llu ms).\n",GetTickCount64()-shutdown_started);
    DWORD lifecycle_before, lifecycle_after; GetProcessHandleCount(GetCurrentProcess(),&lifecycle_before);
    for (unsigned round = 0; round < 30; ++round) {
        if (!win_backend_init(&backend) || win_backend_init(&backend)) return 18;
        if (round % 2 && !win_scan_host_start(&backend,&interfaces[selected],L"localhost",&repeat_port,1)) return 23;
        win_backend_shutdown(&backend);
        ULONGLONG limit = GetTickCount64()+2000;
        while (!win_backend_release(&backend)) { if (GetTickCount64() > limit) return 19; Sleep(1); }
        if (!win_backend_release(&backend)) return 20;
    }
    GetProcessHandleCount(GetCurrentProcess(),&lifecycle_after);
    if (lifecycle_after != lifecycle_before) return 21;
    printf("Backend lifecycle passed: 30 init/shutdown cycles (15 with a scan coordinator); handles %lu -> %lu.\n",lifecycle_before,lifecycle_after);
    WSACleanup();
    return 0;
}
