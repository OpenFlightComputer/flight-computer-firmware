#include "quad_x_mixer.h"

#include <stddef.h>

static bool factor_is_valid(float value)
{
    return (value >= 0.0F) && (value <= 1.0F);
}

static float clamp_throttle(float value)
{
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

bool quad_x_mixer_config_is_valid(const quad_x_mixer_config_t *config)
{
    return (config != NULL) && factor_is_valid(config->roll_factor) &&
           factor_is_valid(config->pitch_factor) &&
           factor_is_valid(config->yaw_factor);
}

bool quad_x_mixer_apply(const quad_x_mixer_config_t *config,
                        propeller_layout_t layout,
                        const receiver_control_snapshot_t *control,
                        uint64_t timestamp_us,
                        motor_command_t *command)
{
    float throttles[MOTOR_COMMAND_MOTOR_COUNT] = {0.0F};
    float roll;
    float pitch;
    float yaw;

    if (!quad_x_mixer_config_is_valid(config) ||
        (layout >= PROPELLER_LAYOUT_COUNT) || (control == NULL) ||
        !control->valid || (command == NULL) ||
        (control->roll < -1.0F) || (control->roll > 1.0F) ||
        (control->pitch < -1.0F) || (control->pitch > 1.0F) ||
        (control->yaw < -1.0F) || (control->yaw > 1.0F) ||
        (control->throttle < 0.0F) || (control->throttle > 1.0F)) {
        return false;
    }

    if (control->throttle == 0.0F) {
        return motor_command_create(command, throttles, timestamp_us) ==
               MOTOR_COMMAND_CREATE_OK;
    }

    roll = control->roll * config->roll_factor;
    pitch = control->pitch * config->pitch_factor;
    yaw = control->yaw * config->yaw_factor;
    if (layout == PROPELLER_LAYOUT_PROPS_OUT) {
        yaw = -yaw;
    }

    /* Logical order: front-left, rear-left, front-right, rear-right. */
    throttles[0] = clamp_throttle(control->throttle + roll - pitch + yaw);
    throttles[1] = clamp_throttle(control->throttle + roll + pitch - yaw);
    throttles[2] = clamp_throttle(control->throttle - roll - pitch - yaw);
    throttles[3] = clamp_throttle(control->throttle - roll + pitch + yaw);

    return motor_command_create(command, throttles, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK;
}

const char *propeller_layout_name(propeller_layout_t layout)
{
    switch (layout) {
    case PROPELLER_LAYOUT_PROPS_IN:
        return "PROPS_IN";
    case PROPELLER_LAYOUT_PROPS_OUT:
        return "PROPS_OUT";
    case PROPELLER_LAYOUT_COUNT:
        break;
    }
    return "INVALID";
}
