#include "level_calibration.h"

#include "accelerometer_attitude.h"

#include <math.h>
#include <stddef.h>

#define LEVEL_CALIBRATION_MAXIMUM_DURATION_US UINT64_C(10000000)
#define LEVEL_CALIBRATION_MAXIMUM_SAMPLES UINT32_C(10000)

bool level_calibration_config_is_valid(
    const level_calibration_config_t *config)
{
    return (config != NULL) && (config->sample_duration_us > 0U) &&
           (config->sample_duration_us <=
            LEVEL_CALIBRATION_MAXIMUM_DURATION_US) &&
           (config->minimum_sample_count > 0U) &&
           (config->minimum_sample_count <=
            LEVEL_CALIBRATION_MAXIMUM_SAMPLES) &&
           isfinite(config->maximum_rate_dps) &&
           (config->maximum_rate_dps > 0.0F) &&
           isfinite(config->maximum_acceleration_standard_deviation_g) &&
           (config->maximum_acceleration_standard_deviation_g > 0.0F) &&
           (config->maximum_acceleration_standard_deviation_g <= 1.0F) &&
           isfinite(config->maximum_acceleration_magnitude_error_g) &&
           (config->maximum_acceleration_magnitude_error_g > 0.0F) &&
           (config->maximum_acceleration_magnitude_error_g <= 1.0F) &&
           isfinite(config->maximum_trim_degrees) &&
           (config->maximum_trim_degrees > 0.0F) &&
           (config->maximum_trim_degrees <= 45.0F) &&
           isfinite(config->acceleration_counts_per_g) &&
           (config->acceleration_counts_per_g > 0.0F) &&
           isfinite(config->gyroscope_counts_per_dps) &&
           (config->gyroscope_counts_per_dps > 0.0F);
}

bool level_calibration_initialize(level_calibration_t *calibration,
                                  const level_calibration_config_t *config,
                                  bool calibrated,
                                  float roll_trim_degrees,
                                  float pitch_trim_degrees)
{
    if ((calibration == NULL) || !level_calibration_config_is_valid(config) ||
        !isfinite(roll_trim_degrees) || !isfinite(pitch_trim_degrees) ||
        (fabsf(roll_trim_degrees) > config->maximum_trim_degrees) ||
        (fabsf(pitch_trim_degrees) > config->maximum_trim_degrees)) {
        return false;
    }
    *calibration = (level_calibration_t){
        .config = *config,
        .state = calibrated ? LEVEL_CALIBRATION_READY
                            : LEVEL_CALIBRATION_UNCALIBRATED,
        .roll_trim_degrees = roll_trim_degrees,
        .pitch_trim_degrees = pitch_trim_degrees,
        .initialized = true,
    };
    return true;
}

bool level_calibration_start(level_calibration_t *calibration,
                             uint64_t now_us)
{
    if ((calibration == NULL) || !calibration->initialized ||
        (calibration->state == LEVEL_CALIBRATION_COLLECTING) ||
        (calibration->state ==
         LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE)) {
        return false;
    }
    calibration->state = LEVEL_CALIBRATION_COLLECTING;
    calibration->started_at_us = now_us;
    calibration->last_sequence = 0U;
    calibration->sample_count = 0U;
    calibration->sequence_seen = false;
    for (size_t axis = 0U; axis < 3U; axis++) {
        calibration->acceleration_sum[axis] = 0.0F;
        calibration->acceleration_squared_sum[axis] = 0.0F;
    }
    return true;
}

static bool movement_detected(const level_calibration_t *calibration,
                              const imu_sample_snapshot_t *sample,
                              const int32_t gyroscope_bias[3])
{
    for (size_t axis = 0U; axis < 3U; axis++) {
        const int32_t raw = axis == 0U ? sample->gyroscope_x
                            : axis == 1U ? sample->gyroscope_y
                                        : sample->gyroscope_z;
        const float rate = (float)((int64_t)raw - gyroscope_bias[axis]) /
                           calibration->config.gyroscope_counts_per_dps;

        if (!isfinite(rate) ||
            (fabsf(rate) > calibration->config.maximum_rate_dps)) {
            return true;
        }
    }
    return false;
}

static bool acceleration_is_plausible(
    const level_calibration_t *calibration,
    const float acceleration_g[3])
{
    const float magnitude = sqrtf((acceleration_g[0] * acceleration_g[0]) +
                                  (acceleration_g[1] * acceleration_g[1]) +
                                  (acceleration_g[2] * acceleration_g[2]));

    return isfinite(magnitude) &&
           (fabsf(magnitude - 1.0F) <=
            calibration->config.maximum_acceleration_magnitude_error_g);
}

static bool variance_is_acceptable(const level_calibration_t *calibration)
{
    const float count = (float)calibration->sample_count;
    const float maximum_variance =
        calibration->config.maximum_acceleration_standard_deviation_g *
        calibration->config.maximum_acceleration_standard_deviation_g;

    for (size_t axis = 0U; axis < 3U; axis++) {
        const float mean = calibration->acceleration_sum[axis] / count;
        float variance =
            (calibration->acceleration_squared_sum[axis] / count) -
            (mean * mean);

        if (variance < 0.0F) {
            variance = 0.0F;
        }
        if (!isfinite(variance)) {
            return false;
        }
        if (variance > maximum_variance) {
            return false;
        }
    }
    return true;
}

static void finish(level_calibration_t *calibration)
{
    float average[3];
    accelerometer_attitude_t attitude;

    if (!variance_is_acceptable(calibration)) {
        calibration->state = LEVEL_CALIBRATION_FAILED_VARIANCE;
        return;
    }
    for (size_t axis = 0U; axis < 3U; axis++) {
        average[axis] = calibration->acceleration_sum[axis] /
                        (float)calibration->sample_count;
    }
    if (!accelerometer_attitude_calculate(average, &attitude) ||
        (fabsf(attitude.roll_degrees) >
         calibration->config.maximum_trim_degrees) ||
        (fabsf(attitude.pitch_degrees) >
         calibration->config.maximum_trim_degrees)) {
        calibration->state = LEVEL_CALIBRATION_FAILED_TRIM;
        return;
    }
    calibration->roll_trim_degrees = attitude.roll_degrees;
    calibration->pitch_trim_degrees = attitude.pitch_degrees;
    calibration->state = LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE;
}

void level_calibration_process(level_calibration_t *calibration,
                               const imu_sample_snapshot_t *sample,
                               imu_freshness_t freshness,
                               const int32_t gyroscope_bias[3],
                               uint64_t now_us)
{
    float acceleration_g[3];

    if ((calibration == NULL) || !calibration->initialized ||
        (calibration->state != LEVEL_CALIBRATION_COLLECTING)) {
        return;
    }
    if ((sample == NULL) || !sample->valid ||
        (freshness != IMU_FRESHNESS_FRESH) || (gyroscope_bias == NULL) ||
        (now_us < calibration->started_at_us)) {
        calibration->state = LEVEL_CALIBRATION_FAILED_INPUT;
        return;
    }
    if (calibration->sequence_seen &&
        (sample->sequence <= calibration->last_sequence)) {
        return;
    }
    calibration->last_sequence = sample->sequence;
    calibration->sequence_seen = true;
    if (movement_detected(calibration, sample, gyroscope_bias)) {
        calibration->state = LEVEL_CALIBRATION_FAILED_MOVEMENT;
        return;
    }
    acceleration_g[0] = (float)sample->acceleration_x /
                        calibration->config.acceleration_counts_per_g;
    acceleration_g[1] = (float)sample->acceleration_y /
                        calibration->config.acceleration_counts_per_g;
    acceleration_g[2] = (float)sample->acceleration_z /
                        calibration->config.acceleration_counts_per_g;
    if (!acceleration_is_plausible(calibration, acceleration_g)) {
        calibration->state = LEVEL_CALIBRATION_FAILED_ACCELERATION;
        return;
    }
    if (calibration->sample_count >= LEVEL_CALIBRATION_MAXIMUM_SAMPLES) {
        calibration->state = LEVEL_CALIBRATION_FAILED_INPUT;
        return;
    }
    for (size_t axis = 0U; axis < 3U; axis++) {
        calibration->acceleration_sum[axis] += acceleration_g[axis];
        calibration->acceleration_squared_sum[axis] +=
            acceleration_g[axis] * acceleration_g[axis];
    }
    calibration->sample_count++;
    if ((now_us - calibration->started_at_us) >=
        calibration->config.sample_duration_us) {
        if (calibration->sample_count <
            calibration->config.minimum_sample_count) {
            calibration->state = LEVEL_CALIBRATION_FAILED_INPUT;
        } else {
            finish(calibration);
        }
    }
}

uint32_t level_calibration_progress_permille(
    const level_calibration_t *calibration,
    uint64_t now_us)
{
    uint64_t elapsed;

    if ((calibration == NULL) || !calibration->initialized) {
        return 0U;
    }
    if ((calibration->state == LEVEL_CALIBRATION_READY) ||
        (calibration->state ==
         LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE)) {
        return 1000U;
    }
    if ((calibration->state != LEVEL_CALIBRATION_COLLECTING) ||
        (now_us < calibration->started_at_us)) {
        return 0U;
    }
    elapsed = now_us - calibration->started_at_us;
    if (elapsed >= calibration->config.sample_duration_us) {
        return 1000U;
    }
    return (uint32_t)((elapsed * UINT64_C(1000)) /
                      calibration->config.sample_duration_us);
}

const char *level_calibration_state_name(level_calibration_state_t state)
{
    switch (state) {
    case LEVEL_CALIBRATION_UNCALIBRATED:
        return "UNCALIBRATED";
    case LEVEL_CALIBRATION_READY:
        return "READY";
    case LEVEL_CALIBRATION_COLLECTING:
        return "COLLECTING";
    case LEVEL_CALIBRATION_RESULT_PENDING_PERSISTENCE:
        return "SAVING";
    case LEVEL_CALIBRATION_FAILED_INPUT:
        return "FAILED_INPUT";
    case LEVEL_CALIBRATION_FAILED_MOVEMENT:
        return "FAILED_MOVEMENT";
    case LEVEL_CALIBRATION_FAILED_ACCELERATION:
        return "FAILED_ACCELERATION";
    case LEVEL_CALIBRATION_FAILED_VARIANCE:
        return "FAILED_VARIANCE";
    case LEVEL_CALIBRATION_FAILED_TRIM:
        return "FAILED_TRIM";
    case LEVEL_CALIBRATION_FAILED_STORAGE:
        return "FAILED_STORAGE";
    case LEVEL_CALIBRATION_STATE_COUNT:
        break;
    }
    return "INVALID";
}
