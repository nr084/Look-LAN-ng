#ifndef WIN_RESOLVER_H
#define WIN_RESOLVER_H
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
typedef bool (*resolver_cancel_t)(void *context);
DWORD win_resolve_ipv4(const wchar_t *name, uint32_t *ip, uint32_t timeout_ms, resolver_cancel_t cancel, void *context);
DWORD win_resolve_name(uint32_t ip, uint32_t interface_index, wchar_t *name, size_t capacity, uint32_t timeout_ms, resolver_cancel_t cancel, void *context);
#endif
