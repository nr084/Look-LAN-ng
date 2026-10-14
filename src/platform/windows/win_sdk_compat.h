#ifndef LOOKLAN_WIN_SDK_COMPAT_H
#define LOOKLAN_WIN_SDK_COMPAT_H
/* Some LLVM-MinGW SDKs omit these Windows 8+ async API declarations.
   ABI follows the locally installed Windows SDK windns.h / ws2tcpip.h.
   The OS still owns resolution and cancellation; no synchronous fallback. */
#ifdef __MINGW32__
#ifdef LOOKLAN_MINGW_DNS_COMPAT
typedef struct {
    ULONG Version;
    PCWSTR QueryName;
    WORD QueryType;
    ULONG64 QueryOptions;
    PVOID pDnsServerList;
    ULONG InterfaceIndex;
    PDNS_QUERY_COMPLETION_ROUTINE pQueryCompletionCallback;
    PVOID pQueryContext;
} DNS_QUERY_REQUEST, *PDNS_QUERY_REQUEST;
typedef struct DECLSPEC_ALIGN(8) { CHAR Reserved[32]; } DNS_QUERY_CANCEL, *PDNS_QUERY_CANCEL;
DNS_STATUS WINAPI DnsQueryEx(DNS_QUERY_REQUEST *,DNS_QUERY_RESULT *,DNS_QUERY_CANCEL *);
DNS_STATUS WINAPI DnsCancelQuery(DNS_QUERY_CANCEL *);
_Static_assert(sizeof(DNS_QUERY_CANCEL)==32,"DNS cancellation ABI");
#if defined(_WIN64)
_Static_assert(sizeof(DNS_QUERY_REQUEST)==64 && sizeof(DNS_QUERY_RESULT)==32,"64-bit DNS query ABI");
#endif
#endif
WINSOCK_API_LINKAGE INT WSAAPI GetAddrInfoExCancel(LPHANDLE);
WINSOCK_API_LINKAGE INT WSAAPI GetAddrInfoExOverlappedResult(LPOVERLAPPED);
#endif
#endif
