#include "imu_processing_pipeline.h"

#include "accelerometer_attitude.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#define MAXIMUM_PIPELINE_GAP_US UINT32_C(1000000)

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

bool imu_processing_config_is_valid(const imu_processing_config_t *config)
{
    return (config != NULL) &&
           acceleration_filter_config_is_valid(
               &config->acceleration_filter) &&
           gyro_filter_config_is_valid(&config->gyro_filter) &&
           attitude_estimator_config_is_valid(&config->attitude_estimator) &&
           (config->maximum_gap_us > 0U) &&
           (config->maximum_gap_us <= MAXIMUM_PIPELINE_GAP_US) &&
           isfinite(config->acceleration_counts_per_g) &&
           (config->acceleration_counts_per_g > 0.0F) &&
           isfinite(config->gyroscope_counts_per_dps) &&
           (config->gyroscope_counts_per_dps > 0.0F) &&
           isfinite(config->level_roll_trim_degrees) &&
           isfinite(config->level_pitch_trim_degrees) &&
           (fabsf(config->level_roll_trim_degrees) <= 45.0F) &&
           (fabsf(config->level_pitch_trim_degrees) <= 45.0F);
}

bool imu_processing_pipeline_initialize(
    imu_processing_pipeline_t *pipeline,
    const imu_processing_config_t *config)
{
    if ((pipeline == NULL) || !imu_processing_config_is_valid(config)) {
        return false;
    }
    *pipeline = (imu_processing_pipeline_t){
        .config = *config,
    };
    if (!acceleration_filter_initialize(&pipeline->acceleration_filter,
                                        &config->acceleration_filter) ||
        !gyro_filter_initialize(&pipeline->gyro_filter,
                                &config->gyro_filter) ||
        !attitude_estimator_initialize(&pipeline->attitude_estimator,
                                       &config->attitude_estimator)) {
        *pipeline = (imu_processing_pipeline_t){0};
        return false;
    }
    pipeline->initialized = true;
    return true;
}

static void invalidate(imu_processing_pipeline_t *pipeline)
{
    pipeline->latest.valid = false;
    pipeline->latest_observation.valid = false;
    pipeline->sample_seen = false;
    acceleration_filter_reset(&pipeline->acceleration_filter);
    gyro_filter_reset(&pipeline->gyro_filter);
    attitude_estimator_reset(&pipeline->attitude_estimator);
}

imu_processing_result_t imu_processing_pipeline_process(
    imu_processing_pipeline_t *pipeline,
    const imu_sample_snapshot_t *sample,
    imu_freshness_t freshness,
    const int32_t gyroscope_bias[3])
{
    float unfiltered_acceleration_g[3];
    float filtered_acceleration_g[3];
    float corrected_dps[3];
    float filtered_dps[3];
    float roll_degrees;
    float pitch_degrees;
    float dt_seconds = 0.0F;
    int32_t acceleration_raw[3];
    int32_t gyroscope_raw[3];
    accelerometer_attitude_t unfiltered_accelerometer_attitude;
    accelerometer_attitude_t filtered_accelerometer_attitude;
    bool continuity_reset = false;
    size_t axis;

    if ((pipeline == NULL) || !pipeline->initialized || (sample == NULL) ||
        (gyroscope_bias == NULL)) {
        return IMU_PROCESSING_INVALID_ARGUMENT;
    }
    acceleration_raw[0] = sample->acceleration_x;
    acceleration_raw[1] = sample->acceleration_y;
    acceleration_raw[2] = sample->acceleration_z;
    gyroscope_raw[0] = sample->gyroscope_x;
    gyroscope_raw[1] = sample->gyroscope_y;
    gyroscope_raw[2] = sample->gyroscope_z;
    if (!sample->valid || (freshness != IMU_FRESHNESS_FRESH)) {
        invalidate(pipeline);
        saturating_increment(&pipeline->statistics.rejected_sample_count);
        return IMU_PROCESSING_INPUT_UNUSABLE;
    }
    if (pipeline->sample_seen &&
        (sample->sequence == pipeline->previous_sequence)) {
        saturating_increment(&pipeline->statistics.duplicate_sample_count);
        return IMU_PROCESSING_NO_NEW_SAMPLE;
    }
    if (pipeline->sample_seen &&
        ((sample->sequence < pipeline->previous_sequence) ||
         (sample->acquired_at_us <= pipeline->previous_timestamp_us))) {
        invalidate(pipeline);
        saturating_increment(&pipeline->statistics.rejected_sample_count);
        return IMU_PROCESSING_INPUT_UNUSABLE;
    }
    if (pipeline->sample_seen) {
        const uint64_t gap_us =
            sample->acquired_at_us - pipeline->previous_timestamp_us;

        if (gap_us > pipeline->config.maximum_gap_us) {
            acceleration_filter_reset(&pipeline->acceleration_filter);
            gyro_filter_reset(&pipeline->gyro_filter);
            attitude_estimator_reset(&pipeline->attitude_estimator);
            continuity_reset = true;
        } else {
            dt_seconds = (float)gap_us * 0.000001F;
        }
    }
    for (axis = 0U; axis < 3U; axis++) {
        unfiltered_acceleration_g[axis] =
            (float)acceleration_raw[axis] /
            pipeline->config.acceleration_counts_per_g;
        corrected_dps[axis] =
            (float)((int64_t)gyroscope_raw[axis] -
                    (int64_t)gyroscope_bias[axis]) /
            pipeline->config.gyroscope_counts_per_dps;
    }
    if (!acceleration_filter_process(&pipeline->acceleration_filter,
                                     unfiltered_acceleration_g,
                                     dt_seconds,
                                     filtered_acceleration_g) ||
        !accelerometer_attitude_calculate(
            unfiltered_acceleration_g,
            &unfiltered_accelerometer_attitude) ||
        !accelerometer_attitude_calculate(
            filtered_acceleration_g,
            &filtered_accelerometer_attitude)) {
        invalidate(pipeline);
        saturating_increment(&pipeline->statistics.rejected_sample_count);
        return IMU_PROCESSING_INPUT_UNUSABLE;
    }
    unfiltered_accelerometer_attitude.roll_degrees -=
        pipeline->config.level_roll_trim_degrees;
    unfiltered_accelerometer_attitude.pitch_degrees -=
        pipeline->config.level_pitch_trim_degrees;
    filtered_accelerometer_attitude.roll_degrees -=
        pipeline->config.level_roll_trim_degrees;
    filtered_accelerometer_attitude.pitch_degrees -=
        pipeline->config.level_pitch_trim_degrees;
    if (!gyro_filter_process(&pipeline->gyro_filter,
                             corrected_dps,
                             dt_seconds,
                             filtered_dps) ||
        !attitude_estimator_process(&pipeline->attitude_estimator,
                                    &filtered_accelerometer_attitude,
                                    filtered_dps,
                                    dt_seconds,
                                    &roll_degrees,
                                    &pitch_degrees)) {
        invalidate(pipeline);
        saturating_increment(&pipeline->statistics.rejected_sample_count);
        return IMU_PROCESSING_INPUT_UNUSABLE;
    }

    pipeline->latest = (attitude_snapshot_t){
        .roll_degrees = roll_degrees,
        .pitch_degrees = pitch_degrees,
        .acquired_at_us = sample->acquired_at_us,
        .source_sequence = sample->sequence,
        .valid = true,
    };
    for (axis = 0U; axis < 3U; axis++) {
        pipeline->latest.acceleration_g[axis] = filtered_acceleration_g[axis];
        pipeline->latest.corrected_gyroscope_dps[axis] = corrected_dps[axis];
        pipeline->latest.filtered_gyroscope_dps[axis] = filtered_dps[axis];
    }
    pipeline->latest_observation = (imu_processing_observation_t){
        .unfiltered_acceleration_magnitude_g = sqrtf(
            (unfiltered_acceleration_g[0] * unfiltered_acceleration_g[0]) +
            (unfiltered_acceleration_g[1] * unfiltered_acceleration_g[1]) +
            (unfiltered_acceleration_g[2] * unfiltered_acceleration_g[2])),
        .filtered_acceleration_magnitude_g = sqrtf(
            (filtered_acceleration_g[0] * filtered_acceleration_g[0]) +
            (filtered_acceleration_g[1] * filtered_acceleration_g[1]) +
            (filtered_acceleration_g[2] * filtered_acceleration_g[2])),
        .unfiltered_accelerometer_attitude_degrees = {
            unfiltered_accelerometer_attitude.roll_degrees,
            unfiltered_accelerometer_attitude.pitch_degrees,
        },
        .filtered_accelerometer_attitude_degrees = {
            filtered_accelerometer_attitude.roll_degrees,
            filtered_accelerometer_attitude.pitch_degrees,
        },
        .gyro_predicted_attitude_degrees = {
            pipeline->attitude_estimator.predicted_roll_degrees,
            pipeline->attitude_estimator.predicted_pitch_degrees,
        },
        .accelerometer_weight =
            pipeline->attitude_estimator.accelerometer_weight,
        .source_sequence = sample->sequence,
        .valid = true,
    };
    for (axis = 0U; axis < 3U; axis++) {
        pipeline->latest_observation.raw_acceleration[axis] =
            acceleration_raw[axis];
        pipeline->latest_observation.raw_gyroscope[axis] =
            gyroscope_raw[axis];
        pipeline->latest_observation.unfiltered_acceleration_g[axis] =
            unfiltered_acceleration_g[axis];
        pipeline->latest_observation.filtered_acceleration_g[axis] =
            filtered_acceleration_g[axis];
        pipeline->latest_observation.corrected_gyroscope_dps[axis] =
            corrected_dps[axis];
        pipeline->latest_observation.filtered_gyroscope_dps[axis] =
            filtered_dps[axis];
    }
    pipeline->previous_sequence = sample->sequence;
    pipeline->previous_timestamp_us = sample->acquired_at_us;
    pipeline->sample_seen = true;
    saturating_increment(&pipeline->statistics.processed_sample_count);
    if (continuity_reset) {
        saturating_increment(&pipeline->statistics.continuity_reset_count);
    }
    return IMU_PROCESSING_UPDATED;
}

bool imu_processing_pipeline_latest_observation(
    const imu_processing_pipeline_t *pipeline,
    imu_processing_observation_t *observation)
{
    if ((pipeline == NULL) || !pipeline->initialized ||
        (observation == NULL)) {
        return false;
    }
    *observation = pipeline->latest_observation;
    return observation->valid;
}

bool imu_processing_pipeline_latest(
    const imu_processing_pipeline_t *pipeline,
    attitude_snapshot_t *snapshot)
{
    if ((pipeline == NULL) || !pipeline->initialized ||
        (snapshot == NULL)) {
        return false;
    }
    *snapshot = pipeline->latest;
    return snapshot->valid;
}
