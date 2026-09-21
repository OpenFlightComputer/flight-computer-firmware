#include "flight_control.h"

#include <stddef.h>

void flight_control_reset(flight_control_stabilization_t *stabilization)
{
    if (stabilization == NULL) {
        return;
    }
    if (stabilization->core != NULL) {
        flight_control_core_reset(stabilization->core);
    }
    if (stabilization->desired_rates != NULL) {
        *stabilization->desired_rates = (flight_control_desired_rates_t){0};
    }
    if (stabilization->rate_output != NULL) {
        *stabilization->rate_output = (rate_controller_output_t){0};
    }
    if (stabilization->rate_result != NULL) {
        *stabilization->rate_result =
            (uint32_t)RATE_CONTROLLER_RESULT_DISABLED;
    }
}

flight_control_result_t flight_control_enter_failsafe(
    flight_control_stabilization_t *stabilization,
    flight_control_result_t accepted_result)
{
    flight_control_reset(stabilization);
    return motor_control_enter_failsafe() == MOTOR_CONTROL_FAILSAFE_ACCEPTED
               ? accepted_result
               : FLIGHT_CONTROL_FAILSAFE_ERROR;
}

static void publish_core_observation(
    flight_control_stabilization_t *stabilization,
    const flight_control_output_t *output)
{
    size_t axis;

    if (stabilization->desired_rates != NULL) {
        *stabilization->desired_rates = (flight_control_desired_rates_t){0};
        for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
            stabilization->desired_rates->desired_rate_dps[axis] =
                output->desired_rate_dps[axis];
        }
        stabilization->desired_rates->valid =
            output->rate_result == RATE_CONTROLLER_RESULT_SEEDED ||
            output->rate_result == RATE_CONTROLLER_RESULT_UPDATED;
    }
    if (stabilization->rate_output != NULL) {
        *stabilization->rate_output = output->rate_output;
    }
    if (stabilization->rate_result != NULL) {
        *stabilization->rate_result = (uint32_t)output->rate_result;
    }
}

flight_control_result_t flight_control_execute_objective(
    motor_control_source_t authority,
    const control_objective_t *objective,
    flight_control_stabilization_t *stabilization,
    flight_control_output_t *output,
    uint64_t now_us)
{
    flight_control_core_result_t core_result;

    if (output != NULL) {
        *output = (flight_control_output_t){0};
    }
    if ((authority == MOTOR_CONTROL_SOURCE_NONE) ||
        (motor_control_active_source() != authority)) {
        flight_control_reset(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    if ((objective == NULL) || (stabilization == NULL) ||
        (stabilization->core == NULL) ||
        (stabilization->profile == NULL) || (output == NULL)) {
        return flight_control_enter_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    core_result = flight_control_update(
        stabilization->core,
        stabilization->profile,
        stabilization->vehicle_state,
        objective,
        now_us,
        output);
    if (core_result == FLIGHT_CONTROL_CORE_WAITING_FOR_STATE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (core_result != FLIGHT_CONTROL_CORE_UPDATED) {
        return flight_control_enter_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    publish_core_observation(stabilization, output);
    return motor_control_submit(authority, &output->mixer_output.command) ==
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
