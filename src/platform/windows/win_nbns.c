#include "win_nbns.h"
#include <ws2tcpip.h>
#include "nbns.h"
static volatile LONG transaction_counter;
bool win_nbns_name(uint32_t source, uint32_t target, char name[256], uint32_t timeout, resolver_cancel_t cancel, void *context) {
    if (!name || !cancel || cancel(context)) return false;
    SOCKET socket_handle = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if (socket_handle == INVALID_SOCKET) return false;
    SOCKADDR_IN local = {0}, remote = {0}; local.sin_family = remote.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(source); remote.sin_addr.s_addr = htonl(target); remote.sin_port = htons(137);
    u_long nonblocking = 1; bool found = false;
    if (bind(socket_handle,(SOCKADDR *)&local,sizeof(local)) || ioctlsocket(socket_handle,FIONBIO,&nonblocking) || connect(socket_handle,(SOCKADDR *)&remote,sizeof(remote))) { closesocket(socket_handle); return false; }
    uint16_t transaction = (uint16_t)(InterlockedIncrement(&transaction_counter)^GetCurrentProcessId()); uint8_t request[50]; nbns_request(transaction,request,sizeof(request));
    if (send(socket_handle,(const char *)request,sizeof(request),0) == sizeof(request)) {
        ULONGLONG deadline = GetTickCount64()+timeout;
        while (!cancel(context)) {
            ULONGLONG now = GetTickCount64(); if (now >= deadline) break;
            fd_set read_set; FD_ZERO(&read_set); FD_SET(socket_handle,&read_set); TIMEVAL wait = {0,(long)((deadline-now > 25 ? 25 : deadline-now)*1000)};
            int ready = select(0,&read_set,NULL,NULL,&wait); if (ready == SOCKET_ERROR) break;
            if (!ready) continue;
            uint8_t response[4096]; int bytes = recv(socket_handle,(char *)response,sizeof(response),0); if (bytes <= 0) break;
            char oem[16]; if (!nbns_name(response,(size_t)bytes,transaction,oem)) continue;
            wchar_t wide[32];
            found = MultiByteToWideChar(CP_OEMCP,0,oem,-1,wide,32) && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,name,256,NULL,NULL);
            if (found) break;
        }
    }
    closesocket(socket_handle); return found && !cancel(context);
}
