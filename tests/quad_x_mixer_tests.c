#include "quad_x_mixer.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    quad_x_mixer_config_t config = {
        .roll_factor = 0.25F,
        .pitch_factor = 0.25F,
        .yaw_factor = 0.15F,
    };
    receiver_control_snapshot_t input = {
        .roll = 1.0F,
        .pitch = 0.0F,
        .yaw = 1.0F,
        .throttle = 0.5F,
        .valid = true,
    };
    motor_command_t command;

    assert(quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_IN,
                              &input, 10U, &command));
    assert(close_to(command.throttle[0], 0.9F));
    assert(close_to(command.throttle[1], 0.6F));
    assert(close_to(command.throttle[2], 0.1F));
    assert(close_to(command.throttle[3], 0.4F));

    assert(quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_OUT,
                              &input, 11U, &command));
    assert(close_to(command.throttle[0], 0.6F));
    assert(close_to(command.throttle[1], 0.9F));
    assert(close_to(command.throttle[2], 0.4F));
    assert(close_to(command.throttle[3], 0.1F));

    input.throttle = 0.0F;
    assert(quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_IN,
                              &input, 12U, &command));
    assert(command.throttle[0] == 0.0F);
    assert(command.throttle[1] == 0.0F);
    assert(command.throttle[2] == 0.0F);
    assert(command.throttle[3] == 0.0F);

    input = (receiver_control_snapshot_t){
        .roll = 1.0F,
        .pitch = -1.0F,
        .yaw = 1.0F,
        .throttle = 0.9F,
        .valid = true,
    };
    assert(quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_IN,
                              &input, 13U, &command));
    assert(command.throttle[0] == 1.0F);
    assert(close_to(command.throttle[1], 0.75F));
    assert(close_to(command.throttle[2], 0.75F));
    assert(close_to(command.throttle[3], 0.55F));

    input.throttle = 0.1F;
    input.roll = -1.0F;
    input.pitch = 1.0F;
    input.yaw = -1.0F;
    assert(quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_IN,
                              &input, 14U, &command));
    assert(command.throttle[0] == 0.0F);

    assert(quad_x_mixer_config_is_valid(&config));
    config.yaw_factor = NAN;
    assert(!quad_x_mixer_config_is_valid(&config));
    assert(!quad_x_mixer_apply(&config, PROPELLER_LAYOUT_PROPS_IN,
                               &input, 15U, &command));
    assert(!quad_x_mixer_apply(NULL, PROPELLER_LAYOUT_PROPS_IN,
                               &input, 15U, &command));
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_IN),
                  "PROPS_IN") == 0);
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_OUT),
                  "PROPS_OUT") == 0);
    return 0;
}
