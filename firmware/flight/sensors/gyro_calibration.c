#include "gyro_calibration.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#define GYRO_CALIBRATION_MAX_DURATION_US UINT64_C(10000000)
#define GYRO_CALIBRATION_MAX_SAMPLES UINT32_C(10000)

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static int32_t rounded_positive(float value)
{
    return (int32_t)(value + 0.5F);
}

bool gyro_calibration_config_is_valid(
    const gyro_calibration_config_t *config)
{
    return (config != NULL) &&
           (config->settling_duration_us <=
            GYRO_CALIBRATION_MAX_DURATION_US) &&
           (config->sample_duration_us > 0U) &&
           (config->sample_duration_us <= GYRO_CALIBRATION_MAX_DURATION_US) &&
           (config->minimum_sample_count > 0U) &&
           (config->minimum_sample_count <= GYRO_CALIBRATION_MAX_SAMPLES) &&
           isfinite(config->maximum_rate_dps) &&
           (config->maximum_rate_dps > 0.0F) &&
           isfinite(config->maximum_standard_deviation_dps) &&
           (config->maximum_standard_deviation_dps > 0.0F) &&
           isfinite(config->counts_per_dps) &&
           (config->counts_per_dps > 0.0F) &&
           ((config->maximum_rate_dps * config->counts_per_dps) <=
            (float)INT32_MAX) &&
           ((config->maximum_standard_deviation_dps *
             config->counts_per_dps) <= (float)INT32_MAX);
}

static void begin_settling(gyro_calibration_t *calibration,
                           uint64_t now_us,
                           bool count_restart)
{
    calibration->state = GYRO_CALIBRATION_SETTLING;
    calibration->phase_started_at_us = now_us;
    calibration->sample_count = 0U;
    calibration->sums[0] = 0;
    calibration->sums[1] = 0;
    calibration->sums[2] = 0;
    calibration->squared_sums[0] = 0;
    calibration->squared_sums[1] = 0;
    calibration->squared_sums[2] = 0;
    if (count_restart) {
        saturating_increment(&calibration->restart_count);
    }
}

bool gyro_calibration_initialize(gyro_calibration_t *calibration,
                                 const gyro_calibration_config_t *config,
                                 uint64_t now_us)
{
    if ((calibration == NULL) || !gyro_calibration_config_is_valid(config)) {
        return false;
    }
    *calibration = (gyro_calibration_t){
        .config = *config,
        .maximum_rate_raw = rounded_positive(
            config->maximum_rate_dps * config->counts_per_dps),
        .maximum_standard_deviation_raw = rounded_positive(
            config->maximum_standard_deviation_dps * config->counts_per_dps),
        .initialized = true,
    };
    begin_settling(calibration, now_us, false);
    return true;
}

static bool sample_exceeds_rate(const gyro_calibration_t *calibration,
                                const int32_t values[3])
{
    size_t axis;

    for (axis = 0U; axis < 3U; axis++) {
        const int64_t magnitude = values[axis] < 0
                                      ? -(int64_t)values[axis]
                                      : (int64_t)values[axis];
        if (magnitude > calibration->maximum_rate_raw) {
            return true;
        }
    }
    return false;
}

static bool variance_is_acceptable(const gyro_calibration_t *calibration)
{
    const int64_t count = (int64_t)calibration->sample_count;
    const int64_t maximum =
        (int64_t)calibration->maximum_standard_deviation_raw;
    const int64_t limit = count * count * maximum * maximum;
    size_t axis;

    for (axis = 0U; axis < 3U; axis++) {
        const int64_t numerator =
            (count * calibration->squared_sums[axis]) -
            (calibration->sums[axis] * calibration->sums[axis]);
        if (numerator > limit) {
            return false;
        }
    }
    return true;
}

static int32_t rounded_average(int64_t sum, uint32_t count)
{
    const int64_t half = (int64_t)count / 2;

    return (int32_t)((sum >= 0 ? sum + half : sum - half) /
                     (int64_t)count);
}

void gyro_calibration_process(gyro_calibration_t *calibration,
                              const imu_sample_snapshot_t *sample,
                              imu_freshness_t freshness,
                              uint64_t now_us)
{
    int32_t values[3];
    size_t axis;

    if ((calibration == NULL) || !calibration->initialized ||
        (calibration->state == GYRO_CALIBRATION_READY)) {
        return;
    }
    if ((sample == NULL) || !sample->valid ||
        (freshness != IMU_FRESHNESS_FRESH) ||
        (now_us < calibration->phase_started_at_us)) {
        begin_settling(calibration, now_us, calibration->sample_count > 0U);
        calibration->sequence_seen = false;
        return;
    }
    if (calibration->sequence_seen &&
        (sample->sequence <= calibration->last_sequence)) {
        return;
    }
    calibration->last_sequence = sample->sequence;
    calibration->sequence_seen = true;

    values[0] = sample->gyroscope_x;
    values[1] = sample->gyroscope_y;
    values[2] = sample->gyroscope_z;
    if (sample_exceeds_rate(calibration, values)) {
        begin_settling(calibration, now_us, true);
        return;
    }

    if (calibration->state == GYRO_CALIBRATION_SETTLING) {
        if ((now_us - calibration->phase_started_at_us) <
            calibration->config.settling_duration_us) {
            return;
        }
        calibration->state = GYRO_CALIBRATION_COLLECTING;
        calibration->phase_started_at_us = now_us;
    }

    if (calibration->sample_count >= GYRO_CALIBRATION_MAX_SAMPLES) {
        begin_settling(calibration, now_us, true);
        return;
    }
    for (axis = 0U; axis < 3U; axis++) {
        calibration->sums[axis] += values[axis];
        calibration->squared_sums[axis] +=
            (int64_t)values[axis] * (int64_t)values[axis];
    }
    calibration->sample_count++;

    if (((now_us - calibration->phase_started_at_us) >=
         calibration->config.sample_duration_us) &&
        (calibration->sample_count >=
         calibration->config.minimum_sample_count)) {
        if (!variance_is_acceptable(calibration)) {
            begin_settling(calibration, now_us, true);
            return;
        }
        for (axis = 0U; axis < 3U; axis++) {
            calibration->bias[axis] = rounded_average(
                calibration->sums[axis], calibration->sample_count);
        }
        calibration->state = GYRO_CALIBRATION_READY;
    }
}

bool gyro_calibration_bias(const gyro_calibration_t *calibration,
                           int32_t bias[3])
{
    if ((calibration == NULL) || !calibration->initialized ||
        (calibration->state != GYRO_CALIBRATION_READY) || (bias == NULL)) {
        return false;
    }
    bias[0] = calibration->bias[0];
    bias[1] = calibration->bias[1];
    bias[2] = calibration->bias[2];
    return true;
}

bool gyro_calibration_correct(const gyro_calibration_t *calibration,
                              const imu_sample_snapshot_t *sample,
                              int32_t corrected[3])
{
    if ((sample == NULL) || !sample->valid || (corrected == NULL) ||
        !gyro_calibration_bias(calibration, corrected)) {
        return false;
    }
    corrected[0] = sample->gyroscope_x - corrected[0];
    corrected[1] = sample->gyroscope_y - corrected[1];
    corrected[2] = sample->gyroscope_z - corrected[2];
    return true;
}

uint32_t gyro_calibration_progress_permille(
    const gyro_calibration_t *calibration,
    uint64_t now_us)
{
    uint64_t time_progress;
    uint64_t sample_progress;
    uint64_t elapsed;

    if ((calibration == NULL) || !calibration->initialized) {
        return 0U;
    }
    if (calibration->state == GYRO_CALIBRATION_READY) {
        return 1000U;
    }
    if (calibration->state != GYRO_CALIBRATION_COLLECTING) {
        return 0U;
    }
    elapsed = now_us < calibration->phase_started_at_us
                  ? 0U
                  : now_us - calibration->phase_started_at_us;
    time_progress = elapsed >= calibration->config.sample_duration_us
                        ? 1000U
                        : (elapsed * UINT64_C(1000)) /
                              calibration->config.sample_duration_us;
    sample_progress =
        ((uint64_t)calibration->sample_count * UINT64_C(1000)) /
        calibration->config.minimum_sample_count;
    if (time_progress > 1000U) {
        time_progress = 1000U;
    }
    if (sample_progress > 1000U) {
        sample_progress = 1000U;
    }
    return (uint32_t)(time_progress < sample_progress ? time_progress
                                                      : sample_progress);
}

const char *gyro_calibration_state_name(gyro_calibration_state_t state)
{
    switch (state) {
    case GYRO_CALIBRATION_SETTLING:
        return "SETTLING";
    case GYRO_CALIBRATION_COLLECTING:
        return "COLLECTING";
    case GYRO_CALIBRATION_READY:
        return "READY";
    }
    return "INVALID";
}
