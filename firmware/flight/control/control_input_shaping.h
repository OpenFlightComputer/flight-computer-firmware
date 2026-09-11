#ifndef OPENFLIGHTCOMPUTER_CONTROL_INPUT_SHAPING_H
#define OPENFLIGHTCOMPUTER_CONTROL_INPUT_SHAPING_H

#include "control_curve.h"
#include "receiver_normalization.h"

#include <stdbool.h>

typedef struct {
    float deadband;
    float maximum_angle_degrees;
    float maximum_rate_dps;
    control_curve_config_t curve;
} control_axis_config_t;

typedef struct {
    float zero_deadband;
    float maximum;
    control_curve_config_t curve;
} control_throttle_config_t;

typedef struct {
    control_axis_config_t roll;
    control_axis_config_t pitch;
    control_axis_config_t yaw;
    control_throttle_config_t throttle;
} control_input_shaping_config_t;

typedef struct {
    float deadband;
    float deadband_scale;
    float maximum_angle_degrees;
    float maximum_rate_dps;
    prepared_control_curve_t curve;
} prepared_control_axis_t;

typedef struct {
    float zero_deadband;
    float deadband_scale;
    float maximum;
    prepared_control_curve_t curve;
} prepared_control_throttle_t;

typedef struct {
    prepared_control_axis_t roll;
    prepared_control_axis_t pitch;
    prepared_control_axis_t yaw;
    prepared_control_throttle_t throttle;
    bool initialized;
} prepared_control_input_shaping_t;

typedef struct {
    float roll_normalized;
    float pitch_normalized;
    float yaw_normalized;
    float throttle;
    float desired_roll_degrees;
    float desired_pitch_degrees;
    float desired_yaw_rate_dps;
    bool valid;
} control_setpoint_t;

bool control_input_shaping_config_is_valid(
    const control_input_shaping_config_t *config);
bool control_input_shaping_prepare(
    const control_input_shaping_config_t *config,
    prepared_control_input_shaping_t *prepared);
bool control_input_shaping_apply(
    const prepared_control_input_shaping_t *prepared,
    const receiver_control_snapshot_t *input,
    control_setpoint_t *setpoint);

#endif
