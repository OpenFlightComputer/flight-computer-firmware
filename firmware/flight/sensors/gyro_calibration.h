#ifndef OPENFLIGHTCOMPUTER_GYRO_CALIBRATION_H
#define OPENFLIGHTCOMPUTER_GYRO_CALIBRATION_H

#include "imu_sample.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t settling_duration_us;
    uint64_t sample_duration_us;
    uint32_t minimum_sample_count;
    float maximum_rate_dps;
    float maximum_standard_deviation_dps;
    float counts_per_dps;
} gyro_calibration_config_t;

typedef enum {
    GYRO_CALIBRATION_SETTLING = 0,
    GYRO_CALIBRATION_COLLECTING,
    GYRO_CALIBRATION_READY,
} gyro_calibration_state_t;

typedef struct {
    gyro_calibration_config_t config;
    gyro_calibration_state_t state;
    uint64_t phase_started_at_us;
    uint64_t last_sequence;
    int64_t sums[3];
    int64_t squared_sums[3];
    int32_t bias[3];
    uint32_t sample_count;
    uint32_t restart_count;
    int32_t maximum_rate_raw;
    int32_t maximum_standard_deviation_raw;
    bool sequence_seen;
    bool initialized;
} gyro_calibration_t;

bool gyro_calibration_config_is_valid(
    const gyro_calibration_config_t *config);
bool gyro_calibration_initialize(gyro_calibration_t *calibration,
                                 const gyro_calibration_config_t *config,
                                 uint64_t now_us);
void gyro_calibration_process(gyro_calibration_t *calibration,
                              const imu_sample_snapshot_t *sample,
                              imu_freshness_t freshness,
                              uint64_t now_us);
bool gyro_calibration_bias(const gyro_calibration_t *calibration,
                           int32_t bias[3]);
bool gyro_calibration_correct(const gyro_calibration_t *calibration,
                              const imu_sample_snapshot_t *sample,
                              int32_t corrected[3]);
uint32_t gyro_calibration_progress_permille(
    const gyro_calibration_t *calibration,
    uint64_t now_us);
const char *gyro_calibration_state_name(gyro_calibration_state_t state);

#endif
