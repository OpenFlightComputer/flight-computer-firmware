#ifndef OPENFLIGHTCOMPUTER_BOARD_SD_CARD_H
#define OPENFLIGHTCOMPUTER_BOARD_SD_CARD_H

#include "spi_device.h"

#include <stdbool.h>

spi_device_t *board_sd_card_spi_device(void);
bool board_sd_card_inserted(void);

#endif
