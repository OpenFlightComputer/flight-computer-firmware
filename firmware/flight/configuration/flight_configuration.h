#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_H

#include "motor_configuration.h"
#include "quad_x_mixer.h"
#include "receiver_failsafe.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t schema_version;
    propeller_layout_t propeller_layout;
    motor_configuration_t motors;
    quad_x_mixer_config_t mixer;
    receiver_failsafe_config_t receiver_failsafe;
} flight_configuration_t;

void flight_configuration_defaults(flight_configuration_t *configuration);
bool flight_configuration_is_valid(
    const flight_configuration_t *configuration);

#endif
