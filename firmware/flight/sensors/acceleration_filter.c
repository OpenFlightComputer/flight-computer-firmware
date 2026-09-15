#include "acceleration_filter.h"

#include <math.h>
#include <stddef.h>

#define TWO_PI 6.28318530717958647692F
#define MAXIMUM_CUTOFF_HZ 500.0F

bool acceleration_filter_config_is_valid(
    const acceleration_filter_config_t *config)
{
    return (config != NULL) &&
           (config->type == ACCELERATION_FILTER_FIRST_ORDER_LOW_PASS) &&
           isfinite(config->cutoff_hz) && (config->cutoff_hz > 0.0F) &&
           (config->cutoff_hz <= MAXIMUM_CUTOFF_HZ);
}

bool acceleration_filter_initialize(
    acceleration_filter_t *filter,
    const acceleration_filter_config_t *config)
{
    if ((filter == NULL) || !acceleration_filter_config_is_valid(config)) {
        return false;
    }
    *filter = (acceleration_filter_t){
        .config = *config,
        .time_constant_seconds = 1.0F / (TWO_PI * config->cutoff_hz),
        .initialized = true,
    };
    return true;
}

void acceleration_filter_reset(acceleration_filter_t *filter)
{
    if ((filter != NULL) && filter->initialized) {
        for (size_t axis = 0U; axis < 3U; axis++) {
            filter->output_g[axis] = 0.0F;
        }
        filter->sample_seen = false;
    }
}

bool acceleration_filter_process(acceleration_filter_t *filter,
                                 const float input_g[3],
                                 float dt_seconds,
                                 float output_g[3])
{
    float alpha;
    size_t axis;

    if ((filter == NULL) || !filter->initialized || (input_g == NULL) ||
        (output_g == NULL) || !isfinite(dt_seconds) ||
        (dt_seconds < 0.0F)) {
        return false;
    }
    for (axis = 0U; axis < 3U; axis++) {
        if (!isfinite(input_g[axis])) {
            return false;
        }
    }
    if (!filter->sample_seen) {
        for (axis = 0U; axis < 3U; axis++) {
            filter->output_g[axis] = input_g[axis];
        }
        filter->sample_seen = true;
    } else {
        if (dt_seconds <= 0.0F) {
            return false;
        }
        alpha = dt_seconds /
                (filter->time_constant_seconds + dt_seconds);
        for (axis = 0U; axis < 3U; axis++) {
            filter->output_g[axis] +=
                alpha * (input_g[axis] - filter->output_g[axis]);
        }
    }
    for (axis = 0U; axis < 3U; axis++) {
        output_g[axis] = filter->output_g[axis];
    }
    return true;
}

const char *acceleration_filter_type_name(acceleration_filter_type_t type)
{
    return type == ACCELERATION_FILTER_FIRST_ORDER_LOW_PASS
               ? "FIRST_ORDER_LOW_PASS" : "INVALID";
}
