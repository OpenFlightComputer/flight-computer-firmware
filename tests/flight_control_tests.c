#include "flight_control.h"

#include "motor_control.h"

#include <assert.h>
#include <math.h>
#include <string.h>

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

static control_input_shaping_config_t control_configuration(void)
{
    const control_curve_config_t identity = {
        .type = CONTROL_CURVE_TYPE_CONTROL_POINTS,
        .interpolation = CONTROL_CURVE_INTERPOLATION_LINEAR,
        .point_count = 2U,
        .points = {{0.0F, 0.0F}, {1.0F, 1.0F}},
    };
    return (control_input_shaping_config_t){
        .roll = {
            .maximum_angle_degrees = 30.0F,
            .maximum_rate_dps = 180.0F,
            .curve = identity,
        },
        .pitch = {
            .maximum_angle_degrees = 30.0F,
            .maximum_rate_dps = 180.0F,
            .curve = identity,
        },
        .yaw = {
            .maximum_rate_dps = 150.0F,
            .curve = identity,
        },
        .throttle = {.maximum = 1.0F, .curve = identity},
    };
}

static receiver_failsafe_decision_t live_decision(void)
{
    return (receiver_failsafe_decision_t){
        .action = RECEIVER_FAILSAFE_ACTION_LIVE,
        .requested_control = {
            .roll = 0.4F,
            .pitch = -0.2F,
            .yaw = 0.5F,
            .throttle = 0.5F,
            .valid = true,
        },
    };
}

int main(void)
{
    const control_input_shaping_config_t control_config =
        control_configuration();
    const quad_x_mixer_config_t mixer_config = {
        .roll_factor = 0.25F,
        .pitch_factor = 0.25F,
        .yaw_factor = 0.15F,
    };
    prepared_control_input_shaping_t control;
    prepared_quad_x_mixer_t mixer;
    receiver_failsafe_decision_t decision = live_decision();

    assert(control_input_shaping_prepare(&control_config, &control));
    assert(quad_x_mixer_prepare(&mixer_config, PROPELLER_LAYOUT_PROPS_IN,
                                &mixer));

    active_source = MOTOR_CONTROL_SOURCE_NONE;
    assert(flight_control_process_receiver(&control, &mixer, &decision, NULL, 42U) ==
           FLIGHT_CONTROL_IDLE);
    assert(submit_count == 0U);

    active_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    submit_result = MOTOR_CONTROL_SUBMIT_ACCEPTED;
    assert(flight_control_process_receiver(&control, &mixer, &decision, NULL, 42U) ==
           FLIGHT_CONTROL_SUBMITTED);
    assert(submit_count == 1U);
    assert(submitted_command.valid);
    assert(submitted_command.timestamp_us == 42U);
    assert(fabsf(submitted_command.throttle[0] - 0.725F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[1] - 0.475F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[2] - 0.375F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[3] - 0.425F) < 0.000001F);

    {
        const roll_attitude_controller_config_t roll_controller = {
            .gain_per_s = 4.0F,
        };
        const pitch_attitude_controller_config_t pitch_controller = {
            .gain_per_s = 4.0F,
        };
        const rate_controller_config_t rate_config = {
            .type = RATE_CONTROLLER_TYPE_PID,
            .maximum_gap_us = 10000U,
            .axis = {
                {.kp = 0.01F, .ki = 0.0F, .kd = 0.0F,
                 .integral_limit = 0.2F, .output_limit = 1.0F},
                {.kp = 0.01F, .ki = 0.0F, .kd = 0.0F,
                 .integral_limit = 0.2F, .output_limit = 1.0F},
                {.kp = 0.01F, .ki = 0.0F, .kd = 0.0F,
                 .integral_limit = 0.2F, .output_limit = 1.0F},
            },
        };
        rate_controller_t rate_controller;
        flight_control_desired_rates_t desired_rates;
        rate_controller_output_t rate_output;
        volatile uint32_t rate_result = UINT32_MAX;
        attitude_snapshot_t attitude = {
            .filtered_gyroscope_dps = {10.0F, -5.0F, 2.0F},
            .roll_degrees = 5.0F,
            .pitch_degrees = -5.0F,
            .acquired_at_us = 1000U,
            .source_sequence = 1U,
            .valid = true,
        };
        flight_control_shadow_stabilization_t stabilization;

        assert(rate_controller_initialize(&rate_controller, &rate_config));
        stabilization = (flight_control_shadow_stabilization_t){
            .roll_controller = &roll_controller,
            .pitch_controller = &pitch_controller,
            .rate_controller = &rate_controller,
            .attitude = &attitude,
            .desired_rates = &desired_rates,
            .rate_output = &rate_output,
            .rate_result = &rate_result,
        };
        decision = live_decision();
        assert(flight_control_process_receiver(&control, &mixer, &decision,
                                               &stabilization, 42U) ==
               FLIGHT_CONTROL_SUBMITTED);
        assert(rate_result == (uint32_t)RATE_CONTROLLER_RESULT_SEEDED);
        assert(desired_rates.valid);
        assert(desired_rates.desired_rate_dps[0] == 28.0F);
        assert(desired_rates.desired_rate_dps[1] == -4.0F);
        assert(desired_rates.desired_rate_dps[2] == 75.0F);
        assert(!rate_output.valid);

        attitude.acquired_at_us = 2000U;
        attitude.source_sequence = 2U;
        assert(flight_control_process_receiver(&control, &mixer, &decision,
                                               &stabilization, 43U) ==
               FLIGHT_CONTROL_SUBMITTED);
        assert(rate_result == (uint32_t)RATE_CONTROLLER_RESULT_UPDATED);
        assert(rate_output.valid);
        assert(fabsf(rate_output.axis[0].total - 0.18F) < 0.000001F);
        assert(fabsf(rate_output.axis[1].total - 0.01F) < 0.000001F);
        assert(fabsf(rate_output.axis[2].total - 0.73F) < 0.000001F);
        assert(fabsf(submitted_command.throttle[0] - 0.725F) < 0.000001F);

        attitude.valid = false;
        assert(flight_control_process_receiver(&control, &mixer, &decision,
                                               &stabilization, 44U) ==
               FLIGHT_CONTROL_SUBMITTED);
        assert(rate_result == (uint32_t)RATE_CONTROLLER_RESULT_DISABLED);
        assert(!desired_rates.valid);
        assert(!rate_output.valid);

        attitude.valid = true;
        attitude.acquired_at_us = 3000U;
        attitude.source_sequence = 3U;
        assert(flight_control_process_receiver(&control, &mixer, &decision,
                                               &stabilization, 45U) ==
               FLIGHT_CONTROL_SUBMITTED);
        assert(rate_result == (uint32_t)RATE_CONTROLLER_RESULT_SEEDED);

        decision.requested_control.throttle = 0.0F;
        assert(flight_control_process_receiver(&control, &mixer, &decision,
                                               &stabilization, 46U) ==
               FLIGHT_CONTROL_SUBMITTED);
        assert(rate_result == (uint32_t)RATE_CONTROLLER_RESULT_DISABLED);
        assert(!desired_rates.valid);
        assert(!rate_output.valid);
    }

    decision.action = RECEIVER_FAILSAFE_ACTION_STOP;
    failsafe_result = MOTOR_CONTROL_FAILSAFE_ACCEPTED;
    assert(flight_control_process_receiver(&control, &mixer, &decision, NULL, 43U) ==
           FLIGHT_CONTROL_FAILSAFE_ENTERED);
    assert(failsafe_count == 1U);
    assert(submit_count == 6U);

    failsafe_result = MOTOR_CONTROL_FAILSAFE_TRANSITION_ERROR;
    assert(flight_control_process_receiver(&control, &mixer, &decision, NULL, 44U) ==
           FLIGHT_CONTROL_FAILSAFE_ERROR);
    assert(failsafe_count == 2U);

    decision = live_decision();
    decision.requested_control.valid = false;
    assert(flight_control_process_receiver(&control, &mixer, &decision, NULL, 45U) ==
           FLIGHT_CONTROL_MIX_ERROR);

    {
        control_input_shaping_config_t shaped_config =
            control_configuration();
        prepared_control_input_shaping_t strongly_shaped;

        shaped_config.roll.curve.point_count = 3U;
        shaped_config.roll.curve.points[1] =
            (control_curve_point_t){0.5F, 0.1F};
        shaped_config.roll.curve.points[2] =
            (control_curve_point_t){1.0F, 1.0F};
        assert(control_input_shaping_prepare(&shaped_config,
                                             &strongly_shaped));
        decision = live_decision();
        decision.action = RECEIVER_FAILSAFE_ACTION_STAGE_ONE;
        assert(flight_control_process_receiver(&strongly_shaped, &mixer,
                                               &decision, NULL, 46U) ==
               FLIGHT_CONTROL_SUBMITTED);
        /* Failsafe controls are physical normalized commands, not stick input. */
        assert(fabsf(submitted_command.throttle[0] - 0.725F) < 0.000001F);
    }

    {
        receiver_failsafe_t failsafe = {
            .stage_two_latched = true,
            .recovery_ready = true,
            .initialized = true,
        };

        assert(flight_control_recover_receiver(&failsafe) ==
               FLIGHT_CONTROL_RECOVERED);
        assert(recovery_count == 1U);
        assert(!failsafe.stage_two_latched);
        assert(!failsafe.recovery_ready);
        assert(flight_control_recover_receiver(&failsafe) ==
               FLIGHT_CONTROL_RECOVERY_ERROR);
        assert(recovery_count == 1U);
    }
    return 0;
}
