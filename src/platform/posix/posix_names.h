#ifndef POSIX_NAMES_H
#define POSIX_NAMES_H
#include "posix_net.h"
void posix_name(posix_backend_t *backend,uint32_t ip,char name[256]);
size_t posix_ptr_request(uint32_t ip,uint16_t transaction,uint8_t packet[512]);
bool posix_ptr_reply(const uint8_t *packet,size_t length,uint32_t ip,uint16_t transaction,char name[256]);
bool posix_ptr_lookup(posix_backend_t *backend,uint32_t ip,uint32_t server,unsigned port,unsigned timeout,char name[256]);
size_t posix_address_request(const char *name,uint16_t transaction,uint8_t packet[512]);
bool posix_address_reply(const uint8_t *packet,size_t length,const char *name,uint16_t transaction,uint32_t *ip);
bool posix_forward(posix_backend_t *backend,const char *name,uint32_t *ip);
#endif
