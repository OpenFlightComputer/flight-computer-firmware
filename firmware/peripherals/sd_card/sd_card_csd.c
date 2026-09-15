#include "sd_card_csd.h"

#include <stddef.h>

#define SD_CARD_SECTOR_SIZE UINT32_C(512)

bool sd_card_parse_csd(const uint8_t csd[16], uint64_t *sector_count)
{
    uint8_t structure;

    if ((csd == NULL) || (sector_count == NULL)) {
        return false;
    }
    structure = csd[0] >> 6U;
    if (structure == 1U) {
        const uint32_t size = ((uint32_t)(csd[7] & 0x3FU) << 16U) |
                              ((uint32_t)csd[8] << 8U) | csd[9];
        *sector_count = (uint64_t)(size + 1U) * UINT32_C(1024);
        return true;
    }
    if (structure == 0U) {
        const uint32_t size = ((uint32_t)(csd[6] & 0x03U) << 10U) |
                              ((uint32_t)csd[7] << 2U) | (csd[8] >> 6U);
        const uint32_t multiplier =
            ((uint32_t)(csd[9] & 0x03U) << 1U) | (csd[10] >> 7U);
        const uint32_t read_length = csd[5] & 0x0FU;
        *sector_count = ((uint64_t)(size + 1U) <<
                         (multiplier + 2U + read_length)) /
                        SD_CARD_SECTOR_SIZE;
        return true;
    }
    return false;
}
