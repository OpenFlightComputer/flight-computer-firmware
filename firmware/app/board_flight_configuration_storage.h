#ifndef OPENFLIGHTCOMPUTER_BOARD_FLIGHT_CONFIGURATION_STORAGE_H
#define OPENFLIGHTCOMPUTER_BOARD_FLIGHT_CONFIGURATION_STORAGE_H

#include "flight_configuration_storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY 512U

flight_configuration_storage_t board_flight_configuration_storage(void);
bool board_flight_configuration_snapshot_encode(
    const flight_configuration_t *configuration,
    uint8_t *destination,
    size_t capacity,
    size_t *length);

#endif
