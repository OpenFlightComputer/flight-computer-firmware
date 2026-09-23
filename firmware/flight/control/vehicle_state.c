#include "vehicle_state.h"

#include <stddef.h>

bool vehicle_state_from_imu(
    const attitude_snapshot_t *attitude,
    const imu_sample_snapshot_t *source_sample,
    vehicle_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    *state = (vehicle_state_t){0};
    if ((attitude == NULL) || (source_sample == NULL) ||
        !attitude->valid || !source_sample->valid ||
        (attitude->source_sequence != source_sample->sequence) ||
        (attitude->acquired_at_us != source_sample->acquired_at_us)) {
        return false;
    }

    state->attitude_degrees[RATE_CONTROLLER_AXIS_ROLL] =
        attitude->roll_degrees;
    state->attitude_degrees[RATE_CONTROLLER_AXIS_PITCH] =
        attitude->pitch_degrees;
    state->angular_rate_dps[RATE_CONTROLLER_AXIS_ROLL] =
        attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_ROLL];
    state->angular_rate_dps[RATE_CONTROLLER_AXIS_PITCH] =
        attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_PITCH];
    state->angular_rate_dps[RATE_CONTROLLER_AXIS_YAW] =
        attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_YAW];
    state->acquired_at_us = attitude->acquired_at_us;
    state->source_sequence = attitude->source_sequence;
    state->valid = true;
    return true;
}
