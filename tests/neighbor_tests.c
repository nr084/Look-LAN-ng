#include "win_neighbor.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Neighbor check failed, line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static HANDLE gate;
static volatile LONG active, wrong_binding;
static bool bad_mac;
static DWORD WINAPI fake_resolve(PMIB_IPNET_ROW2 row, const SOCKADDR_INET *source) {
    if (row->InterfaceIndex != 9 || ntohl(row->Address.Ipv4.sin_addr.s_addr) != 2 || ntohl(source->Ipv4.sin_addr.s_addr) != 1) InterlockedIncrement(&wrong_binding);
    InterlockedIncrement(&active); WaitForSingleObject(gate,INFINITE);
    row->PhysicalAddressLength = bad_mac ? 5 : 6;
    memset(row->PhysicalAddress,0x42,6); InterlockedDecrement(&active); return NO_ERROR;
}
#define ResolveIpNetEntry2 fake_resolve
#include "../src/platform/windows/win_neighbor.c"
static bool never_cancel(void *context) { (void)context; return false; }
static bool cancel_running(void *context) { (void)context; return InterlockedCompareExchange(&active,0,0) > 0; }
int main(void) {
    win_neighbor_pool_t pool = {0}; uint8_t mac[6] = {0};
    gate = CreateEventW(NULL,TRUE,TRUE,NULL); CHECK(gate && win_neighbor_init(&pool));
    for (unsigned warm = 0; warm < 20; ++warm) CHECK(win_neighbor_probe(&pool,1,2,9,mac,never_cancel,NULL) == SCAN_OK);
    DWORD before, after; GetProcessHandleCount(GetCurrentProcess(),&before);
    for (unsigned i = 0; i < 100; ++i) CHECK(win_neighbor_probe(&pool,1,2,9,mac,never_cancel,NULL) == SCAN_OK && mac[0] == 0x42);
    GetProcessHandleCount(GetCurrentProcess(),&after); CHECK(after == before && !wrong_binding);
    bad_mac = true; CHECK(win_neighbor_probe(&pool,1,2,9,mac,never_cancel,NULL) == SCAN_UNREACHABLE); bad_mac = false;
    ResetEvent(gate); memset(mac,0x11,6); ULONGLONG started = GetTickCount64();
    CHECK(win_neighbor_probe(&pool,1,2,9,mac,cancel_running,NULL) == SCAN_CANCELLED && mac[0] == 0x11);
    CHECK(GetTickCount64()-started < 500 && active == 1);
    win_neighbor_stop(&pool); CHECK(!win_neighbor_release(&pool));
    CHECK(win_neighbor_probe(&pool,1,2,9,mac,never_cancel,NULL) == SCAN_CANCELLED);
    SetEvent(gate); ULONGLONG deadline = GetTickCount64()+2000;
    while (!win_neighbor_release(&pool)) { CHECK(GetTickCount64() < deadline); Sleep(1); }
    CHECK(!active && !wrong_binding && win_neighbor_release(&pool));
    CloseHandle(gate);
    printf("Neighbor tests passed: bound source/interface, invalid MAC, 100 probes (handles %lu -> %lu), cancellation while API blocked, preserved output and delayed shutdown.\n",before,after); return 0;
}
