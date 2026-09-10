#include "gyro_calibration.h"

#include <assert.h>
#include <stddef.h>

static gyro_calibration_config_t valid_config(void)
{
    return (gyro_calibration_config_t){
        .settling_duration_us = 100U,
        .sample_duration_us = 500U,
        .minimum_sample_count = 5U,
        .maximum_rate_dps = 5.0F,
        .maximum_standard_deviation_dps = 0.5F,
        .counts_per_dps = 16.384F,
    };
}

static imu_sample_snapshot_t sample(uint64_t sequence,
                                    int32_t x,
                                    int32_t y,
                                    int32_t z)
{
    return (imu_sample_snapshot_t){
        .gyroscope_x = x,
        .gyroscope_y = y,
        .gyroscope_z = z,
        .sequence = sequence,
        .valid = true,
    };
}

int main(void)
{
    gyro_calibration_t calibration;
    gyro_calibration_config_t config = valid_config();
    int32_t values[3];
    imu_sample_snapshot_t current;
    size_t index;

    assert(!gyro_calibration_initialize(NULL, &config, 0U));
    config.minimum_sample_count = 0U;
    assert(!gyro_calibration_initialize(&calibration, &config, 0U));
    config = valid_config();
    assert(gyro_calibration_initialize(&calibration, &config, 0U));
    assert(calibration.state == GYRO_CALIBRATION_SETTLING);

    current = sample(1U, 4, -6, 2);
    gyro_calibration_process(&calibration, &current,
                             IMU_FRESHNESS_FRESH, 99U);
    assert(calibration.sample_count == 0U);
    for (index = 0U; index < 5U; index++) {
        current = sample(2U + index, 4, -6, 2);
        gyro_calibration_process(&calibration, &current,
                                 IMU_FRESHNESS_FRESH,
                                 100U + ((uint64_t)index * 125U));
    }
    assert(calibration.state == GYRO_CALIBRATION_READY);
    assert(gyro_calibration_bias(&calibration, values));
    assert(values[0] == 4 && values[1] == -6 && values[2] == 2);
    current = sample(20U, 7, -4, -1);
    assert(gyro_calibration_correct(&calibration, &current, values));
    assert(values[0] == 3 && values[1] == 2 && values[2] == -3);
    assert(gyro_calibration_progress_permille(&calibration, 1000U) == 1000U);

    assert(gyro_calibration_initialize(&calibration, &config, 0U));
    current = sample(1U, 100, 0, 0);
    gyro_calibration_process(&calibration, &current,
                             IMU_FRESHNESS_FRESH, 100U);
    assert(calibration.state == GYRO_CALIBRATION_SETTLING);
    assert(calibration.restart_count == 1U);

    current = sample(2U, 1, 1, 1);
    gyro_calibration_process(&calibration, &current,
                             IMU_FRESHNESS_FRESH, 200U);
    assert(calibration.sample_count == 1U);
    gyro_calibration_process(&calibration, &current,
                             IMU_FRESHNESS_FRESH, 300U);
    assert(calibration.sample_count == 1U);
    gyro_calibration_process(&calibration, &current,
                             IMU_FRESHNESS_LOST, 301U);
    assert(calibration.state == GYRO_CALIBRATION_SETTLING);
    assert(calibration.restart_count == 2U);

    assert(gyro_calibration_initialize(&calibration, &config, 0U));
    for (index = 0U; index < 5U; index++) {
        current = sample(1U + index,
                         (index & 1U) == 0U ? 10 : -10,
                         0,
                         0);
        gyro_calibration_process(&calibration, &current,
                                 IMU_FRESHNESS_FRESH,
                                 100U + ((uint64_t)index * 125U));
    }
    assert(calibration.state == GYRO_CALIBRATION_SETTLING);
    assert(calibration.restart_count == 1U);
    return 0;
}
