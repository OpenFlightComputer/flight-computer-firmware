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

bool quad_x_mixer_prepare(const quad_x_mixer_config_t *config,
                          propeller_layout_t layout,
                          prepared_quad_x_mixer_t *prepared)
{
    float yaw;

    if (!quad_x_mixer_config_is_valid(config) ||
        (layout >= PROPELLER_LAYOUT_COUNT) || (prepared == NULL)) {
        return false;
    }
    yaw = layout == PROPELLER_LAYOUT_PROPS_OUT
              ? -config->yaw_factor : config->yaw_factor;
    *prepared = (prepared_quad_x_mixer_t){
        .coefficient = {
            {config->roll_factor, -config->pitch_factor, yaw},
            {config->roll_factor, config->pitch_factor, -yaw},
            {-config->roll_factor, -config->pitch_factor, -yaw},
            {-config->roll_factor, config->pitch_factor, yaw},
        },
        .initialized = true,
    };
    return true;
}

bool quad_x_mixer_apply_prepared(const prepared_quad_x_mixer_t *prepared,
                                 const receiver_control_snapshot_t *control,
                                 uint64_t timestamp_us,
                                 motor_command_t *command)
{
    float throttles[MOTOR_COMMAND_MOTOR_COUNT] = {0.0F};
    size_t motor;

    if ((prepared == NULL) || !prepared->initialized || (control == NULL) ||
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
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        throttles[motor] = clamp_throttle(
            control->throttle +
            control->roll * prepared->coefficient[motor][0] +
            control->pitch * prepared->coefficient[motor][1] +
            control->yaw * prepared->coefficient[motor][2]);
    }
    return motor_command_create(command, throttles, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK;
}

bool quad_x_mixer_apply(const quad_x_mixer_config_t *config,
                        propeller_layout_t layout,
                        const receiver_control_snapshot_t *control,
                        uint64_t timestamp_us,
                        motor_command_t *command)
{
    prepared_quad_x_mixer_t prepared;

    return quad_x_mixer_prepare(config, layout, &prepared) &&
           quad_x_mixer_apply_prepared(&prepared, control, timestamp_us,
                                       command);
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
