#include "accelerometer_attitude.h"
#include "attitude_estimator.h"
#include "gyro_filter.h"
#include "imu_processing_pipeline.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static bool close_to(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static imu_processing_config_t valid_config(void)
{
    return (imu_processing_config_t){
        .gyro_filter = {
            .type = GYRO_FILTER_FIRST_ORDER_LOW_PASS,
            .cutoff_hz = 80.0F,
        },
        .attitude_estimator = {
            .type = ATTITUDE_ESTIMATOR_COMPLEMENTARY,
            .accelerometer_correction_time_constant_s = 0.5F,
        },
        .maximum_gap_us = 10000U,
        .acceleration_counts_per_g = 16384.0F,
        .gyroscope_counts_per_dps = 16.384F,
    };
}

static void filter_is_bounded_and_replaceable(void)
{
    gyro_filter_t filter;
    gyro_filter_config_t config = valid_config().gyro_filter;
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float step[3] = {100.0F, 0.0F, 0.0F};
    float output[3];

    assert(gyro_filter_initialize(&filter, &config));
    assert(gyro_filter_process(&filter, zero, 0.0F, output));
    assert(gyro_filter_process(&filter, step, 0.001F, output));
    assert(close_to(output[0], 33.45F, 0.1F));
    assert(output[1] == 0.0F);
    assert(strcmp(gyro_filter_type_name(config.type),
                  "FIRST_ORDER_LOW_PASS") == 0);
}

static void accelerometer_provides_level_and_tilt_reference(void)
{
    const float level[3] = {0.0F, 0.0F, -1.0F};
    const float rolled_right[3] = {0.0F, -1.0F, 0.0F};
    accelerometer_attitude_t attitude;

    assert(accelerometer_attitude_calculate(level, &attitude));
    assert(close_to(attitude.roll_degrees, 0.0F, 0.001F));
    assert(close_to(attitude.pitch_degrees, 0.0F, 0.001F));
    assert(accelerometer_attitude_calculate(rolled_right, &attitude));
    assert(close_to(attitude.roll_degrees, 90.0F, 0.001F));
}

static void complementary_estimator_integrates_then_corrects(void)
{
    attitude_estimator_t estimator;
    const attitude_estimator_config_t config =
        valid_config().attitude_estimator;
    accelerometer_attitude_t accelerometer = {0};
    const float rotation[3] = {10.0F, 0.0F, 0.0F};
    const float stopped[3] = {0.0F, 0.0F, 0.0F};
    float roll;
    float pitch;

    assert(attitude_estimator_initialize(&estimator, &config));
    assert(attitude_estimator_process(&estimator, &accelerometer,
                                      rotation, 0.0F, &roll, &pitch));
    assert(attitude_estimator_process(&estimator, &accelerometer,
                                      rotation, 0.001F, &roll, &pitch));
    assert(close_to(roll, 0.00998F, 0.0001F));

    accelerometer.roll_degrees = 30.0F;
    assert(attitude_estimator_process(&estimator, &accelerometer,
                                      stopped, 0.001F, &roll, &pitch));
    assert(roll > 0.06F);
    assert(roll < 0.08F);

    assert(!attitude_estimator_process(&estimator, &accelerometer,
                                       rotation, FLT_MAX, &roll, &pitch));
}

static void pipeline_uses_timestamps_and_resets_large_gaps(void)
{
    imu_processing_pipeline_t pipeline;
    const imu_processing_config_t config = valid_config();
    const int32_t bias[3] = {10, -20, 30};
    imu_sample_snapshot_t sample = {
        .acceleration_z = -16384,
        .gyroscope_x = 10,
        .gyroscope_y = -20,
        .gyroscope_z = 30,
        .acquired_at_us = 1000U,
        .sequence = 1U,
        .valid = true,
    };
    attitude_snapshot_t snapshot;

    assert(imu_processing_pipeline_initialize(&pipeline, &config));
    assert(imu_processing_pipeline_process(&pipeline, &sample,
                                           IMU_FRESHNESS_FRESH, bias) ==
           IMU_PROCESSING_UPDATED);
    assert(imu_processing_pipeline_latest(&pipeline, &snapshot));
    assert(close_to(snapshot.roll_degrees, 0.0F, 0.001F));
    assert(snapshot.filtered_gyroscope_dps[0] == 0.0F);

    assert(imu_processing_pipeline_process(&pipeline, &sample,
                                           IMU_FRESHNESS_FRESH, bias) ==
           IMU_PROCESSING_NO_NEW_SAMPLE);
    assert(pipeline.statistics.duplicate_sample_count == 1U);

    sample.sequence = 2U;
    sample.acquired_at_us = 2000U;
    sample.gyroscope_x = 1648;
    assert(imu_processing_pipeline_process(&pipeline, &sample,
                                           IMU_FRESHNESS_FRESH, bias) ==
           IMU_PROCESSING_UPDATED);
    assert(imu_processing_pipeline_latest(&pipeline, &snapshot));
    assert(snapshot.roll_degrees > 0.03F);
    assert(snapshot.filtered_gyroscope_dps[0] > 30.0F);

    sample.sequence = 3U;
    sample.acquired_at_us = 13000U;
    sample.gyroscope_x = 10;
    assert(imu_processing_pipeline_process(&pipeline, &sample,
                                           IMU_FRESHNESS_FRESH, bias) ==
           IMU_PROCESSING_UPDATED);
    assert(pipeline.statistics.continuity_reset_count == 1U);
    assert(imu_processing_pipeline_latest(&pipeline, &snapshot));
    assert(close_to(snapshot.roll_degrees, 0.0F, 0.001F));

    assert(imu_processing_pipeline_process(&pipeline, &sample,
                                           IMU_FRESHNESS_STALE, bias) ==
           IMU_PROCESSING_INPUT_UNUSABLE);
    assert(!imu_processing_pipeline_latest(&pipeline, &snapshot));
}

static void invalid_configuration_is_rejected(void)
{
    imu_processing_pipeline_t pipeline;
    imu_processing_config_t config = valid_config();

    config.gyro_filter.type = GYRO_FILTER_TYPE_COUNT;
    assert(!imu_processing_pipeline_initialize(&pipeline, &config));
    config = valid_config();
    config.attitude_estimator.type = ATTITUDE_ESTIMATOR_TYPE_COUNT;
    assert(!imu_processing_pipeline_initialize(&pipeline, &config));
    config = valid_config();
    config.maximum_gap_us = 0U;
    assert(!imu_processing_pipeline_initialize(&pipeline, &config));
}

int main(void)
{
    filter_is_bounded_and_replaceable();
    accelerometer_provides_level_and_tilt_reference();
    complementary_estimator_integrates_then_corrects();
    pipeline_uses_timestamps_and_resets_large_gaps();
    invalid_configuration_is_rejected();
    return 0;
}
