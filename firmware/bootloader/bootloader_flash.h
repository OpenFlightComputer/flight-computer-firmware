#ifndef OPENFLIGHTCOMPUTER_BOOTLOADER_FLASH_H
#define OPENFLIGHTCOMPUTER_BOOTLOADER_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool bootloader_flash_begin(void);
bool bootloader_flash_write(uint32_t offset,
                            const uint8_t *data,
                            size_t length);
bool bootloader_flash_commit(uint32_t image_length, uint32_t image_crc32);

#endif
