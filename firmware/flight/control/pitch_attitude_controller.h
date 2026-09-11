#ifndef OPENFLIGHTCOMPUTER_PITCH_ATTITUDE_CONTROLLER_H
#define OPENFLIGHTCOMPUTER_PITCH_ATTITUDE_CONTROLLER_H

#include <stdbool.h>

typedef struct {
    float gain_per_s;
} pitch_attitude_controller_config_t;

bool pitch_attitude_controller_config_is_valid(
    const pitch_attitude_controller_config_t *config);
bool pitch_attitude_controller_update(
    const pitch_attitude_controller_config_t *config,
    float desired_angle_degrees,
    float measured_angle_degrees,
    float maximum_rate_dps,
    float *desired_rate_dps);

#endif
