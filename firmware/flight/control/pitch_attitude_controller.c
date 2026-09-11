#include "pitch_attitude_controller.h"

#include <math.h>
#include <stddef.h>

#define MAXIMUM_ANGLE_GAIN_PER_S 100.0F
#define MAXIMUM_RATE_DPS 2000.0F

static float clamp_rate(float rate_dps, float maximum_rate_dps)
{
    if (rate_dps > maximum_rate_dps) {
        return maximum_rate_dps;
    }
    if (rate_dps < -maximum_rate_dps) {
        return -maximum_rate_dps;
    }
    return rate_dps;
}

bool pitch_attitude_controller_config_is_valid(
    const pitch_attitude_controller_config_t *config)
{
    return (config != NULL) && isfinite(config->gain_per_s) &&
           (config->gain_per_s > 0.0F) &&
           (config->gain_per_s <= MAXIMUM_ANGLE_GAIN_PER_S);
}

bool pitch_attitude_controller_update(
    const pitch_attitude_controller_config_t *config,
    float desired_angle_degrees,
    float measured_angle_degrees,
    float maximum_rate_dps,
    float *desired_rate_dps)
{
    float requested_rate_dps;

    if (!pitch_attitude_controller_config_is_valid(config) ||
        !isfinite(desired_angle_degrees) ||
        !isfinite(measured_angle_degrees) ||
        !isfinite(maximum_rate_dps) || (maximum_rate_dps <= 0.0F) ||
        (maximum_rate_dps > MAXIMUM_RATE_DPS) ||
        (desired_rate_dps == NULL)) {
        return false;
    }
    requested_rate_dps = config->gain_per_s *
                         (desired_angle_degrees - measured_angle_degrees);
    *desired_rate_dps = clamp_rate(requested_rate_dps, maximum_rate_dps);
    return true;
}
