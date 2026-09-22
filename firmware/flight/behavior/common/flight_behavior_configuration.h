#ifndef OPENFLIGHTCOMPUTER_FLIGHT_BEHAVIOR_CONFIGURATION_H
#define OPENFLIGHTCOMPUTER_FLIGHT_BEHAVIOR_CONFIGURATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FLIGHT_BEHAVIOR_MANUAL_EASY = 0,
    FLIGHT_BEHAVIOR_COUNT,
} flight_behavior_id_t;

/*
 * Manual Easy currently has no behavior-specific settings. Keeping a concrete
 * settings type and a tagged union establishes the fixed-capacity extension
 * point for future behaviors without exposing a generic runtime dictionary.
 */
typedef struct {
    uint8_t reserved;
} manual_easy_behavior_settings_t;

typedef union {
    manual_easy_behavior_settings_t manual_easy;
} flight_behavior_settings_t;

typedef struct {
    flight_behavior_id_t id;
    flight_behavior_settings_t settings;
} flight_behavior_configuration_t;

bool flight_behavior_configuration_is_valid(
    const flight_behavior_configuration_t *configuration);

#endif
