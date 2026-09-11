#include "flight_control.h"

#include "motor_control.h"
#include "yaw_attitude_controller.h"

#include <stddef.h>

static void disable_shadow_stabilization(
    flight_control_shadow_stabilization_t *stabilization)
{
    static const float zero_rates[RATE_CONTROLLER_AXIS_COUNT];

    if (stabilization == NULL) {
        return;
    }
    if (stabilization->desired_rates != NULL) {
        *stabilization->desired_rates = (flight_control_desired_rates_t){0};
    }
    if ((stabilization->rate_controller != NULL) &&
        (stabilization->rate_output != NULL)) {
        const rate_controller_result_t result = rate_controller_process(
            stabilization->rate_controller, zero_rates, zero_rates,
            0U, 0U, false, stabilization->rate_output);
        if (stabilization->rate_result != NULL) {
            *stabilization->rate_result = (uint32_t)result;
        }
    }
}

static void update_shadow_stabilization(
    const prepared_control_input_shaping_t *control,
    const control_setpoint_t *setpoint,
    flight_control_shadow_stabilization_t *stabilization)
{
    const attitude_snapshot_t *attitude;
    flight_control_desired_rates_t *desired_rates;
    rate_controller_result_t result;

    if ((stabilization == NULL) || (setpoint->throttle <= 0.0F) ||
        (stabilization->roll_controller == NULL) ||
        (stabilization->pitch_controller == NULL) ||
        (stabilization->rate_controller == NULL) ||
        (stabilization->desired_rates == NULL) ||
        (stabilization->rate_output == NULL) ||
        (stabilization->attitude == NULL) ||
        !stabilization->attitude->valid) {
        disable_shadow_stabilization(stabilization);
        return;
    }
    attitude = stabilization->attitude;
    desired_rates = stabilization->desired_rates;
    if (!roll_attitude_controller_update(
            stabilization->roll_controller,
            setpoint->desired_roll_degrees,
            attitude->roll_degrees,
            control->roll.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_ROLL]) ||
        !pitch_attitude_controller_update(
            stabilization->pitch_controller,
            setpoint->desired_pitch_degrees,
            attitude->pitch_degrees,
            control->pitch.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_PITCH]) ||
        !yaw_attitude_controller_update(
            setpoint->desired_yaw_rate_dps,
            control->yaw.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_YAW])) {
        disable_shadow_stabilization(stabilization);
        return;
    }
    desired_rates->valid = true;
    result = rate_controller_process(
        stabilization->rate_controller,
        desired_rates->desired_rate_dps,
        attitude->filtered_gyroscope_dps,
        attitude->acquired_at_us,
        attitude->source_sequence,
        true,
        stabilization->rate_output);
    if (stabilization->rate_result != NULL) {
        *stabilization->rate_result = (uint32_t)result;
    }
}

flight_control_result_t flight_control_process_receiver(
    const prepared_control_input_shaping_t *control,
    const prepared_quad_x_mixer_t *mixer,
    const receiver_failsafe_decision_t *decision,
    flight_control_shadow_stabilization_t *stabilization,
    uint64_t now_us)
{
    motor_command_t command;
    receiver_control_snapshot_t shaped;
    const receiver_control_snapshot_t *mixer_input;
    control_setpoint_t setpoint;

    if ((control == NULL) || (mixer == NULL) || (decision == NULL) ||
        (motor_control_active_source() != MOTOR_CONTROL_SOURCE_RECEIVER) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_NONE)) {
        disable_shadow_stabilization(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STOP) {
        disable_shadow_stabilization(stabilization);
        return motor_control_enter_failsafe() ==
                       MOTOR_CONTROL_FAILSAFE_ACCEPTED
                   ? FLIGHT_CONTROL_FAILSAFE_ENTERED
                   : FLIGHT_CONTROL_FAILSAFE_ERROR;
    }
    mixer_input = &decision->requested_control;
    if ((decision->action == RECEIVER_FAILSAFE_ACTION_LIVE) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_HOLD_LAST)) {
        if (!control_input_shaping_apply(control,
                                         &decision->requested_control,
                                         &setpoint)) {
            disable_shadow_stabilization(stabilization);
            return FLIGHT_CONTROL_MIX_ERROR;
        }
        shaped = decision->requested_control;
        shaped.roll = setpoint.roll_normalized;
        shaped.pitch = setpoint.pitch_normalized;
        shaped.yaw = setpoint.yaw_normalized;
        shaped.throttle = setpoint.throttle;
        mixer_input = &shaped;
        update_shadow_stabilization(control, &setpoint, stabilization);
    } else {
        disable_shadow_stabilization(stabilization);
    }
    if (!quad_x_mixer_apply_prepared(mixer, mixer_input, now_us, &command)) {
        disable_shadow_stabilization(stabilization);
        return FLIGHT_CONTROL_MIX_ERROR;
    }
    return motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER, &command) ==
                   MOTOR_CONTROL_SUBMIT_ACCEPTED
               ? FLIGHT_CONTROL_SUBMITTED
               : FLIGHT_CONTROL_SUBMIT_ERROR;
}

flight_control_result_t flight_control_recover_receiver(
    receiver_failsafe_t *failsafe)
{
    if ((failsafe == NULL) || !failsafe->initialized ||
        !failsafe->stage_two_latched || !failsafe->recovery_ready) {
        return FLIGHT_CONTROL_RECOVERY_ERROR;
    }
    if (motor_control_recover_to_disarmed() !=
        MOTOR_CONTROL_RECOVERY_ACCEPTED) {
        return FLIGHT_CONTROL_RECOVERY_ERROR;
    }
    return receiver_failsafe_release_stage_two(failsafe)
               ? FLIGHT_CONTROL_RECOVERED
               : FLIGHT_CONTROL_RECOVERY_ERROR;
}
