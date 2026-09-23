#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_SNAPSHOT_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_SNAPSHOT_H

#include "flight_configuration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY 512U

bool flight_configuration_snapshot_encode(
    const flight_configuration_t *configuration,
    uint8_t *destination,
    size_t capacity,
    size_t *length);
bool flight_configuration_snapshot_decode(
    const uint8_t *source,
    size_t length,
    flight_configuration_t *configuration);

#endif
