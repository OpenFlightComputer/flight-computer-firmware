#include "flight_control.h"

#include "motor_control.h"

#include <stddef.h>

flight_control_result_t flight_control_process_receiver(
    const flight_configuration_t *configuration,
    const receiver_failsafe_decision_t *decision,
    uint64_t now_us)
{
    motor_command_t command;

    if ((configuration == NULL) || (decision == NULL) ||
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
    if (!quad_x_mixer_apply(&configuration->mixer,
                            configuration->propeller_layout,
                            &decision->requested_control,
                            now_us,
                            &command)) {
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
