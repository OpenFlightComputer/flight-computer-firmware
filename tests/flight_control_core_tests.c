#include "flight_control_core.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    const rate_controller_config_t rate_config = {
        .type = RATE_CONTROLLER_TYPE_PID,
        .maximum_gap_us = 10000U,
        .integral_activation_throttle = 0.2F,
        .axis = {
            {.kp = 0.01F, .integral_limit = 0.2F, .output_limit = 1.0F},
            {.kp = 0.01F, .integral_limit = 0.2F, .output_limit = 1.0F},
            {.kp = 0.01F, .integral_limit = 0.2F, .output_limit = 1.0F},
        },
    };
    const roll_attitude_controller_config_t roll = {.gain_per_s = 4.0F};
    const pitch_attitude_controller_config_t pitch = {.gain_per_s = 4.0F};
    const float maximum_rate[RATE_CONTROLLER_AXIS_COUNT] = {
        180.0F, 180.0F, 150.0F,
    };
    prepared_quad_x_mixer_t mixer;
    prepared_control_profile_t profile;
    rate_controller_t rate_controller;
    flight_control_core_t core;
    flight_control_output_t output;
    vehicle_state_t state = {
        .attitude_degrees = {5.0F, -5.0F, 0.0F},
        .angular_rate_dps = {10.0F, -5.0F, 2.0F},
        .acquired_at_us = 1000U,
        .source_sequence = 1U,
        .valid = true,
    };
    control_objective_t objective = {
        .axis_mode = {
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_RATE,
        },
        .axis_value = {12.0F, -6.0F, 75.0F},
        .throttle = 0.5F,
        .produced_at_us = 1000U,
        .valid_until_us = 1000U,
        .valid = true,
    };

    assert(quad_x_mixer_prepare(PROPELLER_LAYOUT_PROPS_IN, &mixer));
    assert(rate_controller_initialize(&rate_controller, &rate_config));
    assert(flight_control_profile_prepare(&roll, &pitch, maximum_rate,
                                          &mixer, 0.05F, &profile));
    assert(flight_control_core_initialize(&core, &rate_controller));

    assert(flight_control_update(&core, &profile, &state, &objective,
                                 1000U, &output) ==
           FLIGHT_CONTROL_CORE_UPDATED);
    assert(output.rate_result == RATE_CONTROLLER_RESULT_SEEDED);
    assert(close_to(output.desired_rate_dps[0], 28.0F));
    assert(close_to(output.desired_rate_dps[1], -4.0F));
    assert(close_to(output.desired_rate_dps[2], 75.0F));
    assert(close_to(output.motor_baseline, 0.05F));

    state.acquired_at_us = 2000U;
    state.source_sequence = 2U;
    objective.produced_at_us = 2000U;
    objective.valid_until_us = 2000U;
    objective.axis_mode[RATE_CONTROLLER_AXIS_ROLL] =
        CONTROL_OBJECTIVE_AXIS_RATE;
    objective.axis_mode[RATE_CONTROLLER_AXIS_PITCH] =
        CONTROL_OBJECTIVE_AXIS_RATE;
    objective.axis_value[RATE_CONTROLLER_AXIS_ROLL] = -40.0F;
    objective.axis_value[RATE_CONTROLLER_AXIS_PITCH] = 30.0F;
    assert(flight_control_update(&core, &profile, &state, &objective,
                                 2000U, &output) ==
           FLIGHT_CONTROL_CORE_UPDATED);
    assert(output.rate_result == RATE_CONTROLLER_RESULT_UPDATED);
    assert(close_to(output.desired_rate_dps[0], -40.0F));
    assert(close_to(output.desired_rate_dps[1], 30.0F));
    assert(close_to(output.desired_rate_dps[2], 75.0F));
    assert(output.mixer_output_valid);

    objective.throttle = 0.0F;
    objective.produced_at_us = 3000U;
    objective.valid_until_us = 3000U;
    state.valid = false;
    assert(flight_control_update(&core, &profile, &state, &objective,
                                 3000U, &output) ==
           FLIGHT_CONTROL_CORE_UPDATED);
    assert(output.rate_result == RATE_CONTROLLER_RESULT_DISABLED);
    assert(close_to(output.motor_baseline, 0.05F));
    for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        assert(close_to(output.mixer_output.command.throttle[motor], 0.05F));
    }

    objective.throttle = 0.5F;
    objective.produced_at_us = 4000U;
    objective.valid_until_us = 4000U;
    assert(flight_control_update(&core, &profile, &state, &objective,
                                 4000U, &output) ==
           FLIGHT_CONTROL_CORE_INVALID_INPUT);
    state.valid = true;
    objective.valid_until_us = 3999U;
    assert(flight_control_update(&core, &profile, &state, &objective,
                                 4000U, &output) ==
           FLIGHT_CONTROL_CORE_INVALID_INPUT);
    return 0;
}
