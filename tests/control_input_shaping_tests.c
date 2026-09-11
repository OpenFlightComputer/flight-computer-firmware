#include "control_input_shaping.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static control_curve_config_t curve(void)
{
    return (control_curve_config_t){
        .type = CONTROL_CURVE_TYPE_CONTROL_POINTS,
        .interpolation = CONTROL_CURVE_INTERPOLATION_LINEAR,
        .point_count = 3U,
        .points = {{0.0F, 0.0F}, {0.5F, 0.25F}, {1.0F, 1.0F}},
    };
}

static control_input_shaping_config_t config(void)
{
    const control_axis_config_t axis = {
        .deadband = 0.1F,
        .maximum_angle_degrees = 30.0F,
        .maximum_rate_dps = 200.0F,
        .curve = curve(),
    };
    return (control_input_shaping_config_t){
        .roll = axis,
        .pitch = axis,
        .yaw = {
            .deadband = 0.1F,
            .maximum_angle_degrees = 0.0F,
            .maximum_rate_dps = 150.0F,
            .curve = curve(),
        },
        .throttle = {
            .zero_deadband = 0.05F,
            .maximum = 0.8F,
            .curve = curve(),
        },
    };
}

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    control_input_shaping_config_t source = config();
    prepared_control_input_shaping_t prepared;
    receiver_control_snapshot_t input = {
        .roll = 0.1F,
        .pitch = -0.55F,
        .yaw = 1.0F,
        .throttle = 0.05F,
        .valid = true,
    };
    control_setpoint_t setpoint;
    float output;

    assert(control_curve_config_is_valid(&source.roll.curve));
    assert(control_input_shaping_config_is_valid(&source));
    assert(control_input_shaping_prepare(&source, &prepared));
    assert(control_input_shaping_apply(&prepared, &input, &setpoint));
    assert(setpoint.roll_normalized == 0.0F);
    assert(setpoint.pitch_normalized == 0.0F);
    assert(setpoint.yaw_normalized == 0.0F);
    assert(setpoint.throttle == 0.0F);
    assert(setpoint.desired_pitch_degrees == 0.0F);
    assert(setpoint.desired_yaw_rate_dps == 0.0F);

    input.throttle = 0.525F;
    assert(control_input_shaping_apply(&prepared, &input, &setpoint));
    assert(close_to(setpoint.throttle, 0.2F));
    assert(close_to(setpoint.pitch_normalized, -0.25F));
    assert(close_to(setpoint.yaw_normalized, 1.0F));

    assert(control_curve_apply(&prepared.roll.curve, 0.75F, &output));
    assert(close_to(output, 0.625F));
    assert(strcmp(control_curve_type_name(CONTROL_CURVE_TYPE_CONTROL_POINTS),
                  "CONTROL_POINTS") == 0);
    assert(strcmp(control_curve_interpolation_name(
                      CONTROL_CURVE_INTERPOLATION_LINEAR),
                  "LINEAR") == 0);

    source.roll.curve.points[1].input = 0.0F;
    assert(!control_input_shaping_config_is_valid(&source));
    source = config();
    source.throttle.curve.points[1].output = -0.1F;
    assert(!control_input_shaping_config_is_valid(&source));
    source = config();
    source.pitch.curve.interpolation =
        (control_curve_interpolation_t)99;
    assert(!control_input_shaping_config_is_valid(&source));
    input = (receiver_control_snapshot_t){.throttle = NAN, .valid = true};
    assert(!control_input_shaping_apply(&prepared, &input, &setpoint));
    return 0;
}
