#include "gyro_filter.h"

#include <math.h>
#include <stddef.h>

#define TWO_PI 6.28318530717958647692F
#define MAXIMUM_CUTOFF_HZ 500.0F

bool gyro_filter_config_is_valid(const gyro_filter_config_t *config)
{
    return (config != NULL) &&
           (config->type == GYRO_FILTER_FIRST_ORDER_LOW_PASS) &&
           isfinite(config->cutoff_hz) && (config->cutoff_hz > 0.0F) &&
           (config->cutoff_hz <= MAXIMUM_CUTOFF_HZ);
}

bool gyro_filter_initialize(gyro_filter_t *filter,
                            const gyro_filter_config_t *config)
{
    if ((filter == NULL) || !gyro_filter_config_is_valid(config)) {
        return false;
    }
    *filter = (gyro_filter_t){
        .config = *config,
        .time_constant_seconds = 1.0F / (TWO_PI * config->cutoff_hz),
        .initialized = true,
    };
    return true;
}

void gyro_filter_reset(gyro_filter_t *filter)
{
    if ((filter != NULL) && filter->initialized) {
        filter->output_dps[0] = 0.0F;
        filter->output_dps[1] = 0.0F;
        filter->output_dps[2] = 0.0F;
        filter->sample_seen = false;
    }
}

bool gyro_filter_process(gyro_filter_t *filter,
                         const float input_dps[3],
                         float dt_seconds,
                         float output_dps[3])
{
    float alpha;
    size_t axis;

    if ((filter == NULL) || !filter->initialized || (input_dps == NULL) ||
        (output_dps == NULL) || !isfinite(dt_seconds) ||
        (dt_seconds < 0.0F)) {
        return false;
    }
    for (axis = 0U; axis < 3U; axis++) {
        if (!isfinite(input_dps[axis])) {
            return false;
        }
    }
    if (!filter->sample_seen) {
        for (axis = 0U; axis < 3U; axis++) {
            filter->output_dps[axis] = input_dps[axis];
        }
        filter->sample_seen = true;
    } else {
        if (dt_seconds <= 0.0F) {
            return false;
        }
        alpha = dt_seconds /
                (filter->time_constant_seconds + dt_seconds);
        for (axis = 0U; axis < 3U; axis++) {
            filter->output_dps[axis] +=
                alpha * (input_dps[axis] - filter->output_dps[axis]);
        }
    }
    for (axis = 0U; axis < 3U; axis++) {
        output_dps[axis] = filter->output_dps[axis];
    }
    return true;
}

const char *gyro_filter_type_name(gyro_filter_type_t type)
{
    return type == GYRO_FILTER_FIRST_ORDER_LOW_PASS
               ? "FIRST_ORDER_LOW_PASS" : "INVALID";
}
