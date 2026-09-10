#include "imu_sample.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static void validates_signed_permutation_mapping(void)
{
    imu_axis_mapping_t mapping = {
        .body_x = IMU_AXIS_POSITIVE_Y,
        .body_y = IMU_AXIS_NEGATIVE_X,
        .body_z = IMU_AXIS_NEGATIVE_Z,
    };
    const imu_raw_sample_t raw = {
        .acceleration_x = INT16_MIN,
        .acceleration_y = 2,
        .acceleration_z = -3,
        .gyroscope_x = 4,
        .gyroscope_y = -5,
        .gyroscope_z = 6,
    };
    imu_sample_snapshot_t mapped;

    assert(imu_axis_mapping_is_valid(&mapping));
    assert(imu_map_raw_sample(&mapping, &raw, &mapped));
    assert(mapped.acceleration_x == 2);
    assert(mapped.acceleration_y == INT32_C(32768));
    assert(mapped.acceleration_z == 3);
    assert(mapped.gyroscope_x == -5);
    assert(mapped.gyroscope_y == -4);
    assert(mapped.gyroscope_z == -6);
    assert(mapped.valid);
    assert(mapped.acquired_at_us == 0U);
    assert(mapped.sequence == 0U);

    mapping.body_z = IMU_AXIS_POSITIVE_X;
    assert(!imu_axis_mapping_is_valid(&mapping));
    mapping.body_z = (imu_axis_selection_t)6;
    assert(!imu_axis_mapping_is_valid(&mapping));
    assert(!imu_axis_mapping_is_valid(NULL));
    assert(!imu_map_raw_sample(NULL, &raw, &mapped));
    assert(!imu_map_raw_sample(&mapping, NULL, &mapped));
    assert(!imu_map_raw_sample(&mapping, &raw, NULL));
}

static void classifies_freshness_boundaries(void)
{
    imu_freshness_config_t config = {
        .fresh_through_us = 2000U,
        .lost_after_us = 10000U,
    };
    imu_sample_snapshot_t snapshot = {
        .acquired_at_us = 100U,
        .valid = true,
    };
    uint64_t age;

    assert(imu_freshness_config_is_valid(&config));
    assert(imu_freshness_evaluate(&config, &snapshot, 2100U, &age) ==
           IMU_FRESHNESS_FRESH);
    assert(age == 2000U);
    assert(imu_freshness_evaluate(&config, &snapshot, 2101U, &age) ==
           IMU_FRESHNESS_STALE);
    assert(age == 2001U);
    assert(imu_freshness_evaluate(&config, &snapshot, 10100U, &age) ==
           IMU_FRESHNESS_STALE);
    assert(imu_freshness_evaluate(&config, &snapshot, 10101U, &age) ==
           IMU_FRESHNESS_LOST);
    assert(age == 10001U);
    assert(imu_freshness_evaluate(&config, &snapshot, 99U, &age) ==
           IMU_FRESHNESS_LOST);
    assert(age == UINT64_MAX);

    snapshot.valid = false;
    assert(imu_freshness_evaluate(&config, &snapshot, 100U, &age) ==
           IMU_FRESHNESS_UNAVAILABLE);
    config.fresh_through_us = 0U;
    assert(!imu_freshness_config_is_valid(&config));
    config.fresh_through_us = config.lost_after_us;
    assert(!imu_freshness_config_is_valid(&config));
}

int main(void)
{
    validates_signed_permutation_mapping();
    classifies_freshness_boundaries();
    return 0;
}
