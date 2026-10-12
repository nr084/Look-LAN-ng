#ifndef NBNS_H
#define NBNS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
bool nbns_request(uint16_t transaction, uint8_t *packet, size_t capacity);
/* Raw OEM workstation name, caller converts to UTF-8. */
bool nbns_name(const uint8_t *packet, size_t length, uint16_t transaction, char name[16]);
#endif
