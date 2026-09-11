#ifndef OPENFLIGHTCOMPUTER_ROLL_ATTITUDE_CONTROLLER_H
#define OPENFLIGHTCOMPUTER_ROLL_ATTITUDE_CONTROLLER_H

#include <stdbool.h>

typedef struct {
    float gain_per_s;
} roll_attitude_controller_config_t;

bool roll_attitude_controller_config_is_valid(
    const roll_attitude_controller_config_t *config);
bool roll_attitude_controller_update(
    const roll_attitude_controller_config_t *config,
    float desired_angle_degrees,
    float measured_angle_degrees,
    float maximum_rate_dps,
    float *desired_rate_dps);

#endif
