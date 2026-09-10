#include "attitude_estimator.h"

#include <math.h>
#include <stddef.h>

#define MAXIMUM_CORRECTION_TIME_CONSTANT_S 10.0F

static float wrap_degrees(float value)
{
    return remainderf(value, 360.0F);
}

bool attitude_estimator_config_is_valid(
    const attitude_estimator_config_t *config)
{
    return (config != NULL) &&
           (config->type == ATTITUDE_ESTIMATOR_COMPLEMENTARY) &&
           isfinite(config->accelerometer_correction_time_constant_s) &&
           (config->accelerometer_correction_time_constant_s > 0.0F) &&
           (config->accelerometer_correction_time_constant_s <=
            MAXIMUM_CORRECTION_TIME_CONSTANT_S);
}

bool attitude_estimator_initialize(
    attitude_estimator_t *estimator,
    const attitude_estimator_config_t *config)
{
    if ((estimator == NULL) || !attitude_estimator_config_is_valid(config)) {
        return false;
    }
    *estimator = (attitude_estimator_t){
        .config = *config,
        .initialized = true,
    };
    return true;
}

void attitude_estimator_reset(attitude_estimator_t *estimator)
{
    if ((estimator != NULL) && estimator->initialized) {
        estimator->roll_degrees = 0.0F;
        estimator->pitch_degrees = 0.0F;
        estimator->estimate_seen = false;
    }
}

bool attitude_estimator_process(
    attitude_estimator_t *estimator,
    const accelerometer_attitude_t *accelerometer,
    const float filtered_gyroscope_dps[3],
    float dt_seconds,
    float *roll_degrees,
    float *pitch_degrees)
{
    float accelerometer_weight;
    float predicted_roll;
    float predicted_pitch;

    if ((estimator == NULL) || !estimator->initialized ||
        (accelerometer == NULL) || (filtered_gyroscope_dps == NULL) ||
        (roll_degrees == NULL) || (pitch_degrees == NULL) ||
        !isfinite(accelerometer->roll_degrees) ||
        !isfinite(accelerometer->pitch_degrees) ||
        !isfinite(filtered_gyroscope_dps[0]) ||
        !isfinite(filtered_gyroscope_dps[1]) ||
        !isfinite(dt_seconds) || (dt_seconds < 0.0F)) {
        return false;
    }
    if (!estimator->estimate_seen) {
        estimator->roll_degrees = accelerometer->roll_degrees;
        estimator->pitch_degrees = accelerometer->pitch_degrees;
        estimator->estimate_seen = true;
    } else {
        if (dt_seconds <= 0.0F) {
            return false;
        }
        predicted_roll = wrap_degrees(
            estimator->roll_degrees + filtered_gyroscope_dps[0] * dt_seconds);
        predicted_pitch = wrap_degrees(
            estimator->pitch_degrees + filtered_gyroscope_dps[1] * dt_seconds);
        if (!isfinite(predicted_roll) || !isfinite(predicted_pitch)) {
            return false;
        }
        accelerometer_weight =
            dt_seconds /
            (estimator->config.accelerometer_correction_time_constant_s +
             dt_seconds);
        estimator->roll_degrees = wrap_degrees(
            predicted_roll + accelerometer_weight *
                wrap_degrees(accelerometer->roll_degrees - predicted_roll));
        estimator->pitch_degrees = wrap_degrees(
            predicted_pitch + accelerometer_weight *
                wrap_degrees(accelerometer->pitch_degrees - predicted_pitch));
        if (!isfinite(estimator->roll_degrees) ||
            !isfinite(estimator->pitch_degrees)) {
            return false;
        }
    }
    *roll_degrees = estimator->roll_degrees;
    *pitch_degrees = estimator->pitch_degrees;
    return true;
}

const char *attitude_estimator_type_name(attitude_estimator_type_t type)
{
    return type == ATTITUDE_ESTIMATOR_COMPLEMENTARY
               ? "COMPLEMENTARY" : "INVALID";
}
