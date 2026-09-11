#include "flight_control.h"

#include "motor_control.h"

#include <stddef.h>

flight_control_result_t flight_control_process_receiver(
    const prepared_control_input_shaping_t *control,
    const prepared_quad_x_mixer_t *mixer,
    const receiver_failsafe_decision_t *decision,
    uint64_t now_us)
{
    motor_command_t command;
    receiver_control_snapshot_t shaped;
    const receiver_control_snapshot_t *mixer_input;
    control_setpoint_t setpoint;

    if ((control == NULL) || (mixer == NULL) || (decision == NULL) ||
        (motor_control_active_source() != MOTOR_CONTROL_SOURCE_RECEIVER) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_NONE)) {
        return FLIGHT_CONTROL_IDLE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STOP) {
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
            return FLIGHT_CONTROL_MIX_ERROR;
        }
        shaped = decision->requested_control;
        shaped.roll = setpoint.roll_normalized;
        shaped.pitch = setpoint.pitch_normalized;
        shaped.yaw = setpoint.yaw_normalized;
        shaped.throttle = setpoint.throttle;
        mixer_input = &shaped;
    }
    if (!quad_x_mixer_apply_prepared(mixer, mixer_input, now_us, &command)) {
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
