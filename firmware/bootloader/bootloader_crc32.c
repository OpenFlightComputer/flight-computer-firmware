#include "bootloader_crc32.h"

uint32_t bootloader_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_MAX;
    size_t index;
    uint32_t bit;

    for (index = 0U; index < length; ++index) {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^
                  ((crc & 1U) != 0U ? UINT32_C(0xEDB88320) : 0U);
        }
    }
    return crc ^ UINT32_MAX;
}
