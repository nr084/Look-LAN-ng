#ifndef PORTS_H
#define PORTS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
extern const uint16_t discovery_ports[];
extern const size_t discovery_port_count;
extern const uint16_t quick_ports[];
extern const size_t quick_port_count;
bool ports_parse(const char *text, size_t length, uint16_t *ports, size_t capacity, size_t *count);
const char *port_service(uint16_t port);
#endif
