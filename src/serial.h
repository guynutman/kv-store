#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>
#include <stddef.h>

uint32_t crc32(const uint8_t *buf, size_t len);

#endif 