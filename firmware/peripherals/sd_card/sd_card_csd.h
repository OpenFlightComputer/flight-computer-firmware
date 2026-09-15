#ifndef OPENFLIGHTCOMPUTER_SD_CARD_CSD_H
#define OPENFLIGHTCOMPUTER_SD_CARD_CSD_H

#include <stdbool.h>
#include <stdint.h>

bool sd_card_parse_csd(const uint8_t csd[16], uint64_t *sector_count);

#endif
