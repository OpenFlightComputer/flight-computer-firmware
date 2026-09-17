#ifndef OPENFLIGHTCOMPUTER_BOOTLOADER_CRC32_H
#define OPENFLIGHTCOMPUTER_BOOTLOADER_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t bootloader_crc32(const void *data, size_t length);

#endif
