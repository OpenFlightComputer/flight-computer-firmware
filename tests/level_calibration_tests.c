#include "level_calibration.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static level_calibration_config_t valid_config(void)
{
    return (level_calibration_config_t){
        .sample_duration_us = 400U,
        .minimum_sample_count = 5U,
        .maximum_rate_dps = 3.0F,
        .maximum_acceleration_standard_deviation_g = 0.02F,
        .maximum_acceleration_magnitude_error_g = 0.15F,
        .maximum_trim_degrees = 10.0F,
        .acceleration_counts_per_g = 1000.0F,
        .gyroscope_counts_per_dps = 10.0F,
    };
}

static imu_sample_snapshot_t sample(uint64_t sequence,
                                    int32_t acceleration_x,
                                    int32_t acceleration_y,
                                    int32_t acceleration_z,
                                    int32_t gyroscope_x)
{
    return (imu_sample_snapshot_t){
        .acceleration_x = acceleration_x,
        .acceleration_y = acceleration_y,
        .acceleration_z = acceleration_z,
        .gyroscope_x = gyroscope_x,
        .sequence = sequence,
        .valid = true,
    };
}

static void process_stationary_tilt(level_calibration_t *calibration)
{
    static const int32_t bias[3] = {4, 0, 0};
    size_t index;

    for (index = 0U; index < 5U; index++) {
        const imu_sample_snapshot_t current =
            sample(index + 1U, 35, -52, -998, 4);
        level_calibration_process(calibration, &current,
                                  IMU_FRESHNESS_FRESH, bias,
                                  (uint64_t)index * 100U);
    }
}

int main(void)
{
    level_calibration_t calibration;
    level_calibration_config_t config = valid_config();
    static const int32_t bias[3] = {4, 0, 0};
    imu_sample_snapshot_t current;
    size_t index;

    assert(!level_calibration_initialize(NULL, &config, false, 0.0F, 0.0F));
    config.minimum_sample_count = 0U;
    assert(!level_calibration_initialize(&calibration, &config,
                                         false, 0.0F, 0.0F));
    config = valid_config();
    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(calibration.state == LEVEL_CALIBRATION_UNCALIBRATED);
    assert(level_calibration_start(&calibration, 0U));
    process_stationary_tilt(&calibration);
    assert(calibration.state ==
           LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE);
    assert(fabsf(calibration.roll_trim_degrees - 2.98F) < 0.1F);
    assert(fabsf(calibration.pitch_trim_degrees - 2.01F) < 0.1F);
    assert(level_calibration_progress_permille(&calibration, 400U) == 1000U);

    assert(level_calibration_initialize(&calibration, &config,
                                        true, 1.0F, -2.0F));
    assert(calibration.state == LEVEL_CALIBRATION_READY);
    assert(level_calibration_start(&calibration, 0U));
    current = sample(1U, 0, 0, -1000, 4);
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_FRESH, bias, 0U);
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_FRESH, bias, 100U);
    assert(calibration.sample_count == 1U);

    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(level_calibration_start(&calibration, 0U));
    current = sample(1U, 0, 0, -1000, 40);
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_FRESH, bias, 0U);
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_MOVEMENT);
    assert(calibration.roll_trim_degrees == 0.0F);
    assert(calibration.pitch_trim_degrees == 0.0F);

    assert(level_calibration_initialize(&calibration, &config,
                                        true, 1.0F, -2.0F));
    assert(level_calibration_start(&calibration, 0U));
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_FRESH, bias, 0U);
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_MOVEMENT);
    assert(calibration.roll_trim_degrees == 1.0F);
    assert(calibration.pitch_trim_degrees == -2.0F);

    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(level_calibration_start(&calibration, 0U));
    current = sample(1U, 0, 0, -500, 4);
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_FRESH, bias, 0U);
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_ACCELERATION);

    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(level_calibration_start(&calibration, 0U));
    for (index = 0U; index < 5U; index++) {
        current = sample(index + 1U, 0,
                         (index & 1U) == 0U ? 100 : -100,
                         -995, 4);
        level_calibration_process(&calibration, &current,
                                  IMU_FRESHNESS_FRESH, bias,
                                  (uint64_t)index * 100U);
    }
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_VARIANCE);

    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(level_calibration_start(&calibration, 0U));
    for (index = 0U; index < 5U; index++) {
        current = sample(index + 1U, 259, 0, -966, 4);
        level_calibration_process(&calibration, &current,
                                  IMU_FRESHNESS_FRESH, bias,
                                  (uint64_t)index * 100U);
    }
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_TRIM);

    assert(level_calibration_initialize(&calibration, &config,
                                        false, 0.0F, 0.0F));
    assert(level_calibration_start(&calibration, 100U));
    current = sample(1U, 0, 0, -1000, 4);
    level_calibration_process(&calibration, &current,
                              IMU_FRESHNESS_STALE, bias, 100U);
    assert(calibration.state == LEVEL_CALIBRATION_FAILED_INPUT);
    assert(!level_calibration_start(NULL, 0U));
    assert(level_calibration_state_name(LEVEL_CALIBRATION_READY)[0] == 'R');
    return 0;
}
