#include "flight_control.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static motor_control_source_t active_source;
static motor_control_submit_result_t submit_result;
static motor_control_failsafe_result_t failsafe_result;
static motor_command_t submitted_command;
static uint32_t submit_count;
static uint32_t failsafe_count;
static uint32_t recovery_count;

motor_control_source_t motor_control_active_source(void)
{
    return active_source;
}

motor_control_submit_result_t motor_control_submit(
    motor_control_source_t source,
    const motor_command_t *command)
{
    assert(source == MOTOR_CONTROL_SOURCE_RECEIVER);
    assert(command != NULL);
    submit_count++;
    submitted_command = *command;
    return submit_result;
}

motor_control_failsafe_result_t motor_control_enter_failsafe(void)
{
    failsafe_count++;
    return failsafe_result;
}

motor_control_recovery_result_t motor_control_recover_to_disarmed(void)
{
    recovery_count++;
    return MOTOR_CONTROL_RECOVERY_ACCEPTED;
}

bool receiver_failsafe_release_stage_two(receiver_failsafe_t *failsafe)
{
    if ((failsafe == NULL) || !failsafe->recovery_ready) {
        return false;
    }
    failsafe->stage_two_latched = false;
    failsafe->recovery_ready = false;
    return true;
}

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    const roll_attitude_controller_config_t roll = {.gain_per_s = 4.0F};
    const pitch_attitude_controller_config_t pitch = {.gain_per_s = 4.0F};
    const float maximum_rate[RATE_CONTROLLER_AXIS_COUNT] = {
        180.0F, 180.0F, 150.0F,
    };
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
    prepared_quad_x_mixer_t mixer;
    prepared_control_profile_t profile;
    rate_controller_t rate_controller;
    flight_control_core_t core;
    flight_control_desired_rates_t desired_rates;
    rate_controller_output_t observed_rate_output;
    volatile uint32_t observed_rate_result = UINT32_MAX;
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
        .axis_value = {5.0F, -5.0F, 75.0F},
        .throttle = 0.5F,
        .produced_at_us = 1000U,
        .valid_until_us = 1000U,
        .valid = true,
    };
    flight_control_stabilization_t stabilization;
    flight_control_output_t output;

    assert(quad_x_mixer_prepare(PROPELLER_LAYOUT_PROPS_IN, &mixer));
    assert(rate_controller_initialize(&rate_controller, &rate_config));
    assert(flight_control_profile_prepare(&roll, &pitch, maximum_rate,
                                          &mixer, 0.05F, &profile));
    assert(flight_control_core_initialize(&core, &rate_controller));
    stabilization = (flight_control_stabilization_t){
        .core = &core,
        .profile = &profile,
        .vehicle_state = &state,
        .desired_rates = &desired_rates,
        .rate_output = &observed_rate_output,
        .rate_result = &observed_rate_result,
    };
    submit_result = MOTOR_CONTROL_SUBMIT_ACCEPTED;
    failsafe_result = MOTOR_CONTROL_FAILSAFE_ACCEPTED;

    active_source = MOTOR_CONTROL_SOURCE_NONE;
    assert(flight_control_execute_objective(
               MOTOR_CONTROL_SOURCE_RECEIVER, &objective,
               &stabilization, &output, 1000U) == FLIGHT_CONTROL_IDLE);

    active_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    assert(flight_control_execute_objective(
               MOTOR_CONTROL_SOURCE_RECEIVER, &objective,
               &stabilization, &output, 1000U) ==
           FLIGHT_CONTROL_SUBMITTED);
    assert(output.rate_result == RATE_CONTROLLER_RESULT_SEEDED);
    assert(desired_rates.valid);
    assert(close_to(submitted_command.throttle[0], 0.05F));

    state.acquired_at_us = 2000U;
    state.source_sequence = 2U;
    objective.produced_at_us = 2000U;
    objective.valid_until_us = 2000U;
    assert(flight_control_execute_objective(
               MOTOR_CONTROL_SOURCE_RECEIVER, &objective,
               &stabilization, &output, 2000U) ==
           FLIGHT_CONTROL_SUBMITTED);
    assert(output.rate_result == RATE_CONTROLLER_RESULT_UPDATED);
    assert(submit_count == 2U);

    assert(flight_control_execute_objective(
               MOTOR_CONTROL_SOURCE_RECEIVER, &objective,
               &stabilization, &output, 2000U) ==
           FLIGHT_CONTROL_WAITING_FOR_IMU);
    assert(submit_count == 2U);

    objective.valid = false;
    assert(flight_control_execute_objective(
               MOTOR_CONTROL_SOURCE_RECEIVER, &objective,
               &stabilization, &output, 2000U) ==
           FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    assert(failsafe_count == 1U);

    {
        receiver_failsafe_t failsafe = {
            .stage_two_latched = true,
            .recovery_ready = true,
            .initialized = true,
        };
        assert(flight_control_recover_receiver(&failsafe) ==
               FLIGHT_CONTROL_RECOVERED);
        assert(recovery_count == 1U);
    }
    return 0;
}
