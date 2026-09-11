#include "control_input_shaping.h"

#include <math.h>
#include <stddef.h>

#define MAXIMUM_DEADBAND 0.25F
#define MAXIMUM_ANGLE_DEGREES 85.0F
#define MAXIMUM_RATE_DPS 2000.0F

static bool axis_config_is_valid(const control_axis_config_t *config,
                                 bool angle_required)
{
    return (config != NULL) && isfinite(config->deadband) &&
           (config->deadband >= 0.0F) &&
           (config->deadband <= MAXIMUM_DEADBAND) &&
           isfinite(config->maximum_angle_degrees) &&
           (config->maximum_angle_degrees >= 0.0F) &&
           (config->maximum_angle_degrees <= MAXIMUM_ANGLE_DEGREES) &&
           (!angle_required || (config->maximum_angle_degrees > 0.0F)) &&
           isfinite(config->maximum_rate_dps) &&
           (config->maximum_rate_dps > 0.0F) &&
           (config->maximum_rate_dps <= MAXIMUM_RATE_DPS) &&
           control_curve_config_is_valid(&config->curve);
}

bool control_input_shaping_config_is_valid(
    const control_input_shaping_config_t *config)
{
    return (config != NULL) && axis_config_is_valid(&config->roll, true) &&
           axis_config_is_valid(&config->pitch, true) &&
           axis_config_is_valid(&config->yaw, false) &&
           isfinite(config->throttle.zero_deadband) &&
           (config->throttle.zero_deadband >= 0.0F) &&
           (config->throttle.zero_deadband <= MAXIMUM_DEADBAND) &&
           isfinite(config->throttle.maximum) &&
           (config->throttle.maximum > 0.0F) &&
           (config->throttle.maximum <= 1.0F) &&
           control_curve_config_is_valid(&config->throttle.curve);
}

static bool prepare_axis(const control_axis_config_t *config,
                         prepared_control_axis_t *prepared)
{
    *prepared = (prepared_control_axis_t){
        .deadband = config->deadband,
        .deadband_scale = 1.0F / (1.0F - config->deadband),
        .maximum_angle_degrees = config->maximum_angle_degrees,
        .maximum_rate_dps = config->maximum_rate_dps,
    };
    return control_curve_prepare(&config->curve, &prepared->curve);
}

bool control_input_shaping_prepare(
    const control_input_shaping_config_t *config,
    prepared_control_input_shaping_t *prepared)
{
    if ((prepared == NULL) || !control_input_shaping_config_is_valid(config)) {
        return false;
    }
    *prepared = (prepared_control_input_shaping_t){0};
    if (!prepare_axis(&config->roll, &prepared->roll) ||
        !prepare_axis(&config->pitch, &prepared->pitch) ||
        !prepare_axis(&config->yaw, &prepared->yaw)) {
        return false;
    }
    prepared->throttle = (prepared_control_throttle_t){
        .zero_deadband = config->throttle.zero_deadband,
        .deadband_scale = 1.0F / (1.0F - config->throttle.zero_deadband),
        .maximum = config->throttle.maximum,
    };
    if (!control_curve_prepare(&config->throttle.curve,
                               &prepared->throttle.curve)) {
        *prepared = (prepared_control_input_shaping_t){0};
        return false;
    }
    prepared->initialized = true;
    return true;
}

static bool apply_axis(const prepared_control_axis_t *axis,
                       float input,
                       float *output)
{
    const float magnitude = fabsf(input);
    float curved;
    float adjusted;

    if (!isfinite(input) || (input < -1.0F) || (input > 1.0F)) {
        return false;
    }
    if (magnitude <= axis->deadband) {
        *output = 0.0F;
        return true;
    }
    adjusted = (magnitude - axis->deadband) * axis->deadband_scale;
    if (!control_curve_apply(&axis->curve, adjusted, &curved)) {
        return false;
    }
    *output = input < 0.0F ? -curved : curved;
    return true;
}

bool control_input_shaping_apply(
    const prepared_control_input_shaping_t *prepared,
    const receiver_control_snapshot_t *input,
    control_setpoint_t *setpoint)
{
    float throttle_input;
    float throttle_curved;

    if ((prepared == NULL) || !prepared->initialized || (input == NULL) ||
        !input->valid || (setpoint == NULL) ||
        !isfinite(input->throttle) || (input->throttle < 0.0F) ||
        (input->throttle > 1.0F)) {
        return false;
    }
    *setpoint = (control_setpoint_t){0};
    if (input->throttle <= prepared->throttle.zero_deadband) {
        setpoint->valid = true;
        return true;
    }
    if (!apply_axis(&prepared->roll, input->roll,
                    &setpoint->roll_normalized) ||
        !apply_axis(&prepared->pitch, input->pitch,
                    &setpoint->pitch_normalized) ||
        !apply_axis(&prepared->yaw, input->yaw,
                    &setpoint->yaw_normalized)) {
        return false;
    }
    throttle_input =
        (input->throttle - prepared->throttle.zero_deadband) *
        prepared->throttle.deadband_scale;
    if (!control_curve_apply(&prepared->throttle.curve,
                             throttle_input,
                             &throttle_curved)) {
        return false;
    }
    setpoint->throttle = throttle_curved * prepared->throttle.maximum;
    setpoint->desired_roll_degrees =
        setpoint->roll_normalized * prepared->roll.maximum_angle_degrees;
    setpoint->desired_pitch_degrees =
        setpoint->pitch_normalized * prepared->pitch.maximum_angle_degrees;
    setpoint->desired_yaw_rate_dps =
        setpoint->yaw_normalized * prepared->yaw.maximum_rate_dps;
    setpoint->valid = true;
    return true;
}
