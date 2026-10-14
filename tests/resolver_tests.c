/* Compile the real resolver against deterministic API doubles. No DNS traffic.
   Pending queries complete only on cancellation, exercising storage lifetime. */
#include "win_resolver.h"
#include <ws2tcpip.h>
#include <windns.h>
#include "win_sdk_compat.h"
#include <stdio.h>
#include <wchar.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Resolver check failed, line %d: %s\n",__LINE__,#x); return 1; } } while (0)
enum { SYNC_OK, ASYNC_OK, PENDING, FAIL_QUERY };
static int mode, forward_cancelled, reverse_cancelled, freed_addresses, freed_records, cancel_checks;
static OVERLAPPED *pending_forward;
static DNS_QUERY_REQUEST *pending_request;
static DNS_QUERY_RESULT *pending_answer;
static SOCKADDR_IN address;
static ADDRINFOEXW fake_addresses;
static DNS_RECORDW fake_record;
static int WSAAPI fake_forward(PCWSTR name, PCWSTR service, DWORD ns, LPGUID id, const ADDRINFOEXW *hints, PADDRINFOEXW *result, struct timeval *timeout, LPOVERLAPPED overlapped, LPLOOKUPSERVICE_COMPLETION_ROUTINE callback, LPHANDLE handle) {
    (void)name; (void)service; (void)ns; (void)id; (void)hints; (void)timeout; (void)callback;
    if (mode == FAIL_QUERY) return WSAHOST_NOT_FOUND;
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(0x7f000001);
    fake_addresses.ai_addr = (SOCKADDR *)&address; fake_addresses.ai_addrlen = sizeof(address); *result = &fake_addresses;
    if (mode == SYNC_OK) return 0;
    pending_forward = overlapped; *handle = (HANDLE)1;
    overlapped->Internal = mode == ASYNC_OK ? 0 : WSA_IO_PENDING;
    if (mode == ASYNC_OK) SetEvent(overlapped->hEvent);
    return WSA_IO_PENDING;
}
static int WSAAPI fake_forward_cancel(LPHANDLE handle) {
    ++forward_cancelled; *handle = NULL; pending_forward->Internal = WSA_E_CANCELLED;
    SetEvent(pending_forward->hEvent); return 0;
}
static int WSAAPI fake_forward_result(LPOVERLAPPED overlapped) { return (int)overlapped->Internal; }
static void WSAAPI fake_free_addresses(PADDRINFOEXW result) { if (result) ++freed_addresses; }
static DNS_STATUS WINAPI fake_reverse(PDNS_QUERY_REQUEST request, PDNS_QUERY_RESULT answer, PDNS_QUERY_CANCEL cancel) {
    (void)cancel;
    if (mode == FAIL_QUERY) return DNS_ERROR_RCODE_NAME_ERROR;
    fake_record.wType = DNS_TYPE_PTR; fake_record.Data.PTR.pNameHost = L"router.example.";
    answer->pQueryRecords = &fake_record; answer->QueryStatus = ERROR_SUCCESS;
    if (mode == SYNC_OK) return 0;
    pending_request = request; pending_answer = answer;
    if (mode == ASYNC_OK) request->pQueryCompletionCallback(request->pQueryContext,answer);
    return DNS_REQUEST_PENDING;
}
static DNS_STATUS WINAPI fake_reverse_cancel(PDNS_QUERY_CANCEL cancel) {
    (void)cancel; ++reverse_cancelled; pending_answer->QueryStatus = ERROR_CANCELLED;
    pending_request->pQueryCompletionCallback(pending_request->pQueryContext,pending_answer); return ERROR_SUCCESS;
}
static void WINAPI fake_free_records(PDNS_RECORD records, DNS_FREE_TYPE type) { (void)type; if (records) ++freed_records; }
#define GetAddrInfoExW fake_forward
#define GetAddrInfoExCancel fake_forward_cancel
#define GetAddrInfoExOverlappedResult fake_forward_result
#define FreeAddrInfoExW fake_free_addresses
#define DnsQueryEx fake_reverse
#define DnsCancelQuery fake_reverse_cancel
#undef DnsRecordListFree
#define DnsRecordListFree fake_free_records
#include "../src/platform/windows/win_resolver.c"
static bool already_cancelled(void *context) { (void)context; return true; }
static bool delayed_cancel(void *context) { (void)context; return ++cancel_checks >= 3; }
int main(void) {
    WSADATA data; CHECK(!WSAStartup(MAKEWORD(2,2),&data));
    uint32_t ip = 123; wchar_t name[256] = L"preserved";
    CHECK(win_resolve_ipv4(L"127.0.0.1",&ip,50,NULL,NULL) == ERROR_SUCCESS && ip == 0x7f000001);
    ip = 123;
    CHECK(win_resolve_ipv4(L"host",&ip,50,already_cancelled,NULL) == ERROR_CANCELLED && ip == 123);
    CHECK(win_resolve_name(1,0,name,256,50,already_cancelled,NULL) == ERROR_CANCELLED && !wcscmp(name,L"preserved"));
    for (mode = SYNC_OK; mode <= ASYNC_OK; ++mode) {
        CHECK(!win_resolve_ipv4(L"host",&ip,50,NULL,NULL) && ip == 0x7f000001);
        CHECK(!win_resolve_name(1,0,name,256,50,NULL,NULL) && !wcscmp(name,L"router.example"));
    }
    mode = FAIL_QUERY; ip = 123; wcscpy_s(name,256,L"preserved");
    CHECK(win_resolve_ipv4(L"missing",&ip,50,NULL,NULL) == WSAHOST_NOT_FOUND && ip == 123);
    CHECK(win_resolve_name(1,0,name,256,50,NULL,NULL) == DNS_ERROR_RCODE_NAME_ERROR && !wcscmp(name,L"preserved"));
    mode = SYNC_OK; CHECK(win_resolve_name(1,0,name,4,50,NULL,NULL) == ERROR_INSUFFICIENT_BUFFER);
    mode = PENDING;
    for (unsigned i = 0; i < 10; ++i) {
        ip = 123; cancel_checks = 0;
        CHECK(win_resolve_ipv4(L"pending",&ip,1000,delayed_cancel,NULL) == ERROR_CANCELLED && ip == 123);
        cancel_checks = 0; wcscpy_s(name,256,L"preserved");
        CHECK(win_resolve_name(1,0,name,256,1000,delayed_cancel,NULL) == ERROR_CANCELLED && !wcscmp(name,L"preserved"));
    }
    CHECK(win_resolve_ipv4(L"pending",&ip,5,NULL,NULL) == ERROR_TIMEOUT);
    CHECK(win_resolve_name(1,0,name,256,5,NULL,NULL) == ERROR_TIMEOUT);
    DWORD before, after; GetProcessHandleCount(GetCurrentProcess(),&before);
    for (unsigned i = 0; i < 100; ++i) {
        CHECK(win_resolve_ipv4(L"pending",&ip,1,NULL,NULL) == ERROR_TIMEOUT);
        CHECK(win_resolve_name(1,0,name,256,1,NULL,NULL) == ERROR_TIMEOUT);
    }
    GetProcessHandleCount(GetCurrentProcess(),&after);
    CHECK(after == before && forward_cancelled == 111 && reverse_cancelled == 111);
    CHECK(freed_addresses == 113 && freed_records == 114);
    printf("Resolver tests passed: numeric/sync/async, errors, pending cancellation/timeouts, output preservation, record cleanup; handles %lu -> %lu.\n",before,after);
    WSACleanup(); return 0;
}
