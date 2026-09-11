#include "quad_x_mixer.h"

#include <math.h>
#include <stddef.h>

static bool unit_value_is_valid(float value)
{
    return isfinite(value) && (value >= 0.0F) && (value <= 1.0F);
}

static bool correction_is_valid(float value)
{
    return isfinite(value) && (value >= -1.0F) && (value <= 1.0F);
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

bool quad_x_mixer_prepare(propeller_layout_t layout,
                          prepared_quad_x_mixer_t *prepared)
{
    float yaw;

    if ((layout >= PROPELLER_LAYOUT_COUNT) || (prepared == NULL)) {
        return false;
    }
    yaw = layout == PROPELLER_LAYOUT_PROPS_OUT ? -1.0F : 1.0F;
    *prepared = (prepared_quad_x_mixer_t){
        .coefficient = {
            {1.0F, -1.0F, yaw},
            {1.0F, 1.0F, -yaw},
            {-1.0F, -1.0F, -yaw},
            {-1.0F, 1.0F, yaw},
        },
        .initialized = true,
    };
    return true;
}

bool quad_x_mixer_apply_prepared(const prepared_quad_x_mixer_t *prepared,
                                 float throttle,
                                 const float correction[3],
                                 uint64_t timestamp_us,
                                 quad_x_mixer_output_t *output)
{
    float throttles[MOTOR_COMMAND_MOTOR_COUNT] = {0.0F};
    float delta[MOTOR_COMMAND_MOTOR_COUNT];
    float minimum_delta;
    float maximum_delta;
    float minimum;
    float maximum;
    float scale = 1.0F;
    float shift = 0.0F;
    size_t motor;
    size_t axis;

    if ((prepared == NULL) || !prepared->initialized ||
        !unit_value_is_valid(throttle) || (correction == NULL) ||
        (output == NULL)) {
        return false;
    }
    *output = (quad_x_mixer_output_t){.correction_scale = 1.0F};
    if (throttle == 0.0F) {
        return motor_command_create(&output->command, throttles,
                                    timestamp_us) == MOTOR_COMMAND_CREATE_OK;
    }
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        delta[motor] = 0.0F;
        for (axis = 0U; axis < 3U; axis++) {
            if (!correction_is_valid(correction[axis])) {
                return false;
            }
            delta[motor] += correction[axis] *
                            prepared->coefficient[motor][axis];
        }
    }
    minimum_delta = delta[0];
    maximum_delta = delta[0];
    for (motor = 1U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        if (delta[motor] < minimum_delta) {
            minimum_delta = delta[motor];
        }
        if (delta[motor] > maximum_delta) {
            maximum_delta = delta[motor];
        }
    }
    if ((maximum_delta - minimum_delta) > 1.0F) {
        scale = 1.0F / (maximum_delta - minimum_delta);
        for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            delta[motor] *= scale;
        }
    }
    minimum = throttle + delta[0];
    maximum = minimum;
    for (motor = 1U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        const float value = throttle + delta[motor];
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }
    if (minimum < 0.0F) {
        shift = -minimum;
    } else if (maximum > 1.0F) {
        shift = 1.0F - maximum;
    }
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        throttles[motor] = clamp_throttle(throttle + delta[motor] + shift);
    }
    output->correction_scale = scale;
    output->collective_shift = shift;
    output->saturated = (scale < 1.0F) || (shift != 0.0F);
    return motor_command_create(&output->command, throttles, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK;
}

bool quad_x_mixer_apply(propeller_layout_t layout,
                        float throttle,
                        const float correction[3],
                        uint64_t timestamp_us,
                        quad_x_mixer_output_t *output)
{
    prepared_quad_x_mixer_t prepared;

    return quad_x_mixer_prepare(layout, &prepared) &&
           quad_x_mixer_apply_prepared(&prepared, throttle, correction,
                                       timestamp_us, output);
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
