#ifndef OPENFLIGHTCOMPUTER_IMU_PROCESSING_PIPELINE_H
#define OPENFLIGHTCOMPUTER_IMU_PROCESSING_PIPELINE_H

#include "attitude_estimator.h"
#include "gyro_filter.h"
#include "imu_sample.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    gyro_filter_config_t gyro_filter;
    attitude_estimator_config_t attitude_estimator;
    uint32_t maximum_gap_us;
    float acceleration_counts_per_g;
    float gyroscope_counts_per_dps;
} imu_processing_config_t;

typedef struct {
    float acceleration_g[3];
    float corrected_gyroscope_dps[3];
    float filtered_gyroscope_dps[3];
    float roll_degrees;
    float pitch_degrees;
    uint64_t acquired_at_us;
    uint64_t source_sequence;
    bool valid;
} attitude_snapshot_t;

typedef struct {
    uint32_t processed_sample_count;
    uint32_t duplicate_sample_count;
    uint32_t rejected_sample_count;
    uint32_t continuity_reset_count;
} imu_processing_statistics_t;

typedef enum {
    IMU_PROCESSING_UPDATED = 0,
    IMU_PROCESSING_NO_NEW_SAMPLE,
    IMU_PROCESSING_INPUT_UNUSABLE,
    IMU_PROCESSING_INVALID_ARGUMENT,
} imu_processing_result_t;

typedef struct {
    imu_processing_config_t config;
    gyro_filter_t gyro_filter;
    attitude_estimator_t attitude_estimator;
    attitude_snapshot_t latest;
    imu_processing_statistics_t statistics;
    uint64_t previous_sequence;
    uint64_t previous_timestamp_us;
    bool sample_seen;
    bool initialized;
} imu_processing_pipeline_t;

bool imu_processing_config_is_valid(const imu_processing_config_t *config);
bool imu_processing_pipeline_initialize(
    imu_processing_pipeline_t *pipeline,
    const imu_processing_config_t *config);
imu_processing_result_t imu_processing_pipeline_process(
    imu_processing_pipeline_t *pipeline,
    const imu_sample_snapshot_t *sample,
    imu_freshness_t freshness,
    const int32_t gyroscope_bias[3]);
bool imu_processing_pipeline_latest(
    const imu_processing_pipeline_t *pipeline,
    attitude_snapshot_t *snapshot);

#endif
