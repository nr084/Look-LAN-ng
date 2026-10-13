#include "win_resolver.h"
#include <ws2tcpip.h>
#include <windns.h>
#include "win_sdk_compat.h"
#include <stdio.h>
#include <wchar.h>

/* Keep API-owned storage alive through completion, including after cancellation.
   These waits run only in background workers, never in the UI thread. */
static DWORD wait_query(HANDLE event, uint32_t timeout_ms, resolver_cancel_t cancel, void *context) {
    ULONGLONG deadline = GetTickCount64() + timeout_ms;
    for (;;) {
        if (cancel && cancel(context)) return ERROR_CANCELLED;
        ULONGLONG now = GetTickCount64();
        DWORD wait = now >= deadline ? 0 : (DWORD)(deadline-now > 25 ? 25 : deadline-now);
        DWORD status = WaitForSingleObject(event,wait);
        if (status == WAIT_OBJECT_0) return ERROR_SUCCESS;
        if (status == WAIT_FAILED) return GetLastError();
        if (GetTickCount64() >= deadline) return ERROR_TIMEOUT;
    }
}

DWORD win_resolve_ipv4(const wchar_t *name, uint32_t *ip, uint32_t timeout_ms, resolver_cancel_t cancel, void *context) {
    if (!name || !name[0] || !ip || !timeout_ms) return ERROR_INVALID_PARAMETER;
    if (cancel && cancel(context)) return ERROR_CANCELLED;
    IN_ADDR numeric;
    if (InetPtonW(AF_INET,name,&numeric) == 1) { *ip = ntohl(numeric.s_addr); return ERROR_SUCCESS; }
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
    if (!overlapped.hEvent) return GetLastError();
    ADDRINFOEXW hints = {0}, *addresses = NULL;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    HANDLE query = NULL;
    int error = GetAddrInfoExW(name,NULL,NS_DNS,NULL,&hints,&addresses,NULL,&overlapped,NULL,&query);
    DWORD stopped = ERROR_SUCCESS;
    if (error == WSA_IO_PENDING) {
        stopped = wait_query(overlapped.hEvent,timeout_ms,cancel,context);
        if (stopped != ERROR_SUCCESS) GetAddrInfoExCancel(&query);
        WaitForSingleObject(overlapped.hEvent,INFINITE);
        error = GetAddrInfoExOverlappedResult(&overlapped);
    }
    DWORD result = stopped ? stopped : error ? (DWORD)error : WSAHOST_NOT_FOUND;
    if (cancel && cancel(context)) result = ERROR_CANCELLED;
    else if (!stopped && !error && addresses && addresses->ai_addr && addresses->ai_addrlen >= sizeof(SOCKADDR_IN)) {
        *ip = ntohl(((SOCKADDR_IN *)addresses->ai_addr)->sin_addr.s_addr); result = ERROR_SUCCESS;
    }
    if (addresses) FreeAddrInfoExW(addresses);
    CloseHandle(overlapped.hEvent);
    return result;
}

static void WINAPI dns_complete(void *context, DNS_QUERY_RESULT *result) {
    (void)result;
    SetEvent((HANDLE)context);
}
DWORD win_resolve_name(uint32_t ip, uint32_t interface_index, wchar_t *name, size_t capacity, uint32_t timeout_ms, resolver_cancel_t cancel, void *context) {
    if (!name || !capacity || !timeout_ms) return ERROR_INVALID_PARAMETER;
    if (cancel && cancel(context)) return ERROR_CANCELLED;
    wchar_t query_name[64];
    swprintf_s(query_name,64,L"%u.%u.%u.%u.in-addr.arpa",ip&255,(ip>>8)&255,(ip>>16)&255,ip>>24);
    HANDLE completed = CreateEventW(NULL,TRUE,FALSE,NULL);
    if (!completed) return GetLastError();
    DNS_QUERY_REQUEST request = {0}; DNS_QUERY_RESULT answer = {0}; DNS_QUERY_CANCEL query = {0};
    request.Version = DNS_QUERY_REQUEST_VERSION1; request.QueryName = query_name; request.QueryType = DNS_TYPE_PTR;
    request.InterfaceIndex = interface_index; request.pQueryCompletionCallback = dns_complete; request.pQueryContext = completed;
    answer.Version = DNS_QUERY_RESULTS_VERSION1;
    DWORD result = DnsQueryEx(&request,&answer,&query), stopped = ERROR_SUCCESS;
    if (result == DNS_REQUEST_PENDING) {
        stopped = wait_query(completed,timeout_ms,cancel,context);
        if (stopped != ERROR_SUCCESS) DnsCancelQuery(&query);
        WaitForSingleObject(completed,INFINITE);
        result = answer.QueryStatus;
    }
    if (stopped) result = stopped;
    if (cancel && cancel(context)) result = ERROR_CANCELLED;
    if (!result) {
        result = DNS_INFO_NO_RECORDS;
        for (DNS_RECORDW *record = answer.pQueryRecords; record; record = record->pNext) {
            if (record->wType != DNS_TYPE_PTR || !record->Data.PTR.pNameHost) continue;
            size_t length = wcslen(record->Data.PTR.pNameHost);
            if (length && record->Data.PTR.pNameHost[length-1] == L'.') --length;
            if (length >= capacity) { result = ERROR_INSUFFICIENT_BUFFER; break; }
            wmemcpy(name,record->Data.PTR.pNameHost,length); name[length] = 0; result = ERROR_SUCCESS; break;
        }
    }
    if (answer.pQueryRecords) DnsRecordListFree(answer.pQueryRecords,DnsFreeRecordList);
    CloseHandle(completed);
    return result;
}
