#include "vehicle_state.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    attitude_snapshot_t attitude = {
        .filtered_gyroscope_dps = {1.0F, 2.0F, 3.0F},
        .roll_degrees = 4.0F,
        .pitch_degrees = 5.0F,
        .acquired_at_us = UINT64_C(1234),
        .source_sequence = UINT64_C(8),
        .valid = true,
    };
    imu_sample_snapshot_t sample = {
        .acquired_at_us = UINT64_C(1234),
        .sequence = UINT64_C(8),
        .valid = true,
    };
    vehicle_state_t state;

    assert(vehicle_state_from_imu(&attitude, &sample, &state));
    assert(state.valid);
    assert(state.attitude_degrees[RATE_CONTROLLER_AXIS_ROLL] == 4.0F);
    assert(state.attitude_degrees[RATE_CONTROLLER_AXIS_PITCH] == 5.0F);
    assert(state.angular_rate_dps[RATE_CONTROLLER_AXIS_ROLL] == 1.0F);
    assert(state.angular_rate_dps[RATE_CONTROLLER_AXIS_PITCH] == 2.0F);
    assert(state.angular_rate_dps[RATE_CONTROLLER_AXIS_YAW] == 3.0F);
    assert(state.acquired_at_us == UINT64_C(1234));
    assert(state.source_sequence == UINT64_C(8));

    sample.sequence++;
    assert(!vehicle_state_from_imu(&attitude, &sample, &state));
    assert(!state.valid);
    sample.sequence = attitude.source_sequence;
    sample.acquired_at_us++;
    assert(!vehicle_state_from_imu(&attitude, &sample, &state));
    assert(!state.valid);
    sample.acquired_at_us = attitude.acquired_at_us;
    attitude.valid = false;
    assert(!vehicle_state_from_imu(&attitude, &sample, &state));
    assert(!state.valid);
    assert(!vehicle_state_from_imu(&attitude, &sample, NULL));

    puts("vehicle state tests passed");
    return 0;
}
