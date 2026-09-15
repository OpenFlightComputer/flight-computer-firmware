#ifndef OPENFLIGHTCOMPUTER_LEVEL_CALIBRATION_H
#define OPENFLIGHTCOMPUTER_LEVEL_CALIBRATION_H

#include "imu_sample.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t sample_duration_us;
    uint32_t minimum_sample_count;
    float maximum_rate_dps;
    float maximum_acceleration_standard_deviation_g;
    float maximum_acceleration_magnitude_error_g;
    float maximum_trim_degrees;
    float acceleration_counts_per_g;
    float gyroscope_counts_per_dps;
} level_calibration_config_t;

typedef enum {
    LEVEL_CALIBRATION_UNCALIBRATED = 0,
    LEVEL_CALIBRATION_READY,
    LEVEL_CALIBRATION_COLLECTING,
    LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE,
    LEVEL_CALIBRATION_FAILED_INPUT,
    LEVEL_CALIBRATION_FAILED_MOVEMENT,
    LEVEL_CALIBRATION_FAILED_ACCELERATION,
    LEVEL_CALIBRATION_FAILED_VARIANCE,
    LEVEL_CALIBRATION_FAILED_TRIM,
    LEVEL_CALIBRATION_FAILED_STORAGE,
    LEVEL_CALIBRATION_STATE_COUNT,
} level_calibration_state_t;

typedef struct {
    level_calibration_config_t config;
    level_calibration_state_t state;
    float acceleration_sum[3];
    float acceleration_squared_sum[3];
    float roll_trim_degrees;
    float pitch_trim_degrees;
    uint64_t started_at_us;
    uint64_t last_sequence;
    uint32_t sample_count;
    bool sequence_seen;
    bool initialized;
} level_calibration_t;

bool level_calibration_config_is_valid(
    const level_calibration_config_t *config);
bool level_calibration_initialize(level_calibration_t *calibration,
                                  const level_calibration_config_t *config,
                                  bool calibrated,
                                  float roll_trim_degrees,
                                  float pitch_trim_degrees);
bool level_calibration_start(level_calibration_t *calibration,
                             uint64_t now_us);
void level_calibration_process(level_calibration_t *calibration,
                               const imu_sample_snapshot_t *sample,
                               imu_freshness_t freshness,
                               const int32_t gyroscope_bias[3],
                               uint64_t now_us);
uint32_t level_calibration_progress_permille(
    const level_calibration_t *calibration,
    uint64_t now_us);
const char *level_calibration_state_name(level_calibration_state_t state);

#endif
