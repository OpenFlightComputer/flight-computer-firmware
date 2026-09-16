#include "takeoff_leveling.h"

#include <math.h>
#include <stddef.h>

#define PERMILLE_SCALE 1000.0F
#define DECIDEGREE_SCALE 10.0F
#define MAXIMUM_LEVELING_RATE_DECIDEGREES_PER_SECOND UINT16_C(2000)

static float move_toward_zero(float value, float maximum_change)
{
    if (value > maximum_change) {
        return value - maximum_change;
    }
    if (value < -maximum_change) {
        return value + maximum_change;
    }
    return 0.0F;
}

bool easy_mode_config_is_valid(const easy_mode_config_t *config)
{
    return (config != NULL) && (config->armed_idle_permille > 0U) &&
           (config->armed_idle_permille < 1000U) &&
           (config->activation_throttle_permille > 0U) &&
           (config->activation_throttle_permille < 1000U) &&
           (config->leveling_rate_decidegrees_per_second > 0U) &&
           (config->leveling_rate_decidegrees_per_second <=
            MAXIMUM_LEVELING_RATE_DECIDEGREES_PER_SECOND);
}

float easy_mode_armed_idle(const easy_mode_config_t *config)
{
    return easy_mode_config_is_valid(config)
               ? (float)config->armed_idle_permille / PERMILLE_SCALE
               : 0.0F;
}

float easy_mode_activation_throttle(const easy_mode_config_t *config)
{
    return easy_mode_config_is_valid(config)
               ? (float)config->activation_throttle_permille / PERMILLE_SCALE
               : 0.0F;
}

void takeoff_leveling_initialize(takeoff_leveling_t *leveling)
{
    if (leveling != NULL) {
        *leveling = (takeoff_leveling_t){
            .state = TAKEOFF_LEVELING_DISABLED,
            .initialized = true,
        };
    }
}

void takeoff_leveling_reset(takeoff_leveling_t *leveling,
                            const easy_mode_config_t *config)
{
    if ((leveling == NULL) || !leveling->initialized) {
        return;
    }
    *leveling = (takeoff_leveling_t){
        .state = (config != NULL) && config->takeoff_leveling_enabled
                     ? TAKEOFF_LEVELING_CAPTURING
                     : TAKEOFF_LEVELING_DISABLED,
        .initialized = true,
    };
}

void takeoff_leveling_complete(takeoff_leveling_t *leveling,
                               float pilot_roll_degrees,
                               float pilot_pitch_degrees)
{
    if ((leveling == NULL) || !leveling->initialized) {
        return;
    }
    leveling->launch_roll_offset_degrees = 0.0F;
    leveling->launch_pitch_offset_degrees = 0.0F;
    leveling->effective_roll_degrees = pilot_roll_degrees;
    leveling->effective_pitch_degrees = pilot_pitch_degrees;
    leveling->state = TAKEOFF_LEVELING_COMPLETE;
}

bool takeoff_leveling_apply(takeoff_leveling_t *leveling,
                            const easy_mode_config_t *config,
                            float throttle,
                            float pilot_roll_degrees,
                            float pilot_pitch_degrees,
                            float measured_roll_degrees,
                            float measured_pitch_degrees,
                            uint64_t sample_at_us,
                            float *effective_roll_degrees,
                            float *effective_pitch_degrees)
{
    float maximum_change = 0.0F;

    if ((leveling == NULL) || !leveling->initialized ||
        !easy_mode_config_is_valid(config) || !isfinite(throttle) ||
        (throttle < 0.0F) || (throttle > 1.0F) ||
        !isfinite(pilot_roll_degrees) || !isfinite(pilot_pitch_degrees) ||
        !isfinite(measured_roll_degrees) ||
        !isfinite(measured_pitch_degrees) ||
        (effective_roll_degrees == NULL) ||
        (effective_pitch_degrees == NULL)) {
        return false;
    }
    if (!config->takeoff_leveling_enabled) {
        takeoff_leveling_complete(
            leveling, pilot_roll_degrees, pilot_pitch_degrees);
    } else if (leveling->state == TAKEOFF_LEVELING_DISABLED) {
        takeoff_leveling_reset(leveling, config);
    }

    if (leveling->state == TAKEOFF_LEVELING_CAPTURING) {
        leveling->launch_roll_offset_degrees =
            measured_roll_degrees - pilot_roll_degrees;
        leveling->launch_pitch_offset_degrees =
            measured_pitch_degrees - pilot_pitch_degrees;
        leveling->last_sample_at_us = sample_at_us;
        if (throttle > 0.0F) {
            leveling->state = TAKEOFF_LEVELING_FROZEN;
        }
    }

    if ((leveling->state == TAKEOFF_LEVELING_FROZEN) &&
        (throttle >= easy_mode_activation_throttle(config))) {
        leveling->state = TAKEOFF_LEVELING_ACTIVE;
        leveling->last_sample_at_us = sample_at_us;
    } else if ((leveling->state == TAKEOFF_LEVELING_ACTIVE) &&
               (throttle > 0.0F) &&
               (sample_at_us > leveling->last_sample_at_us)) {
        const uint64_t elapsed_us = sample_at_us - leveling->last_sample_at_us;
        const float leveling_rate_dps =
            (float)config->leveling_rate_decidegrees_per_second /
            DECIDEGREE_SCALE;

        maximum_change = leveling_rate_dps * (float)elapsed_us / 1000000.0F;
        leveling->launch_roll_offset_degrees = move_toward_zero(
            leveling->launch_roll_offset_degrees, maximum_change);
        leveling->launch_pitch_offset_degrees = move_toward_zero(
            leveling->launch_pitch_offset_degrees, maximum_change);
        leveling->last_sample_at_us = sample_at_us;
        if ((leveling->launch_roll_offset_degrees == 0.0F) &&
            (leveling->launch_pitch_offset_degrees == 0.0F)) {
            leveling->state = TAKEOFF_LEVELING_COMPLETE;
        }
    }

    leveling->effective_roll_degrees =
        pilot_roll_degrees + leveling->launch_roll_offset_degrees;
    leveling->effective_pitch_degrees =
        pilot_pitch_degrees + leveling->launch_pitch_offset_degrees;
    *effective_roll_degrees = leveling->effective_roll_degrees;
    *effective_pitch_degrees = leveling->effective_pitch_degrees;
    return true;
}
