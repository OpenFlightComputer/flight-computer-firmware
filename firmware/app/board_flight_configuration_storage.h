#ifndef OPENFLIGHTCOMPUTER_BOARD_FLIGHT_CONFIGURATION_STORAGE_H
#define OPENFLIGHTCOMPUTER_BOARD_FLIGHT_CONFIGURATION_STORAGE_H

#include "flight_configuration_snapshot.h"
#include "flight_configuration_storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

flight_configuration_storage_t board_flight_configuration_storage(void);
bool board_flight_configuration_snapshot_encode(
    const flight_configuration_t *configuration,
    uint8_t *destination,
    size_t capacity,
    size_t *length);

#endif
