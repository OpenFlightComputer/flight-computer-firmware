#include "flight_control.h"

#include "motor_control.h"

#include <stddef.h>

static void reset_stabilization(flight_control_stabilization_t *stabilization)
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
        if (stabilization->rate_result != NULL) {
            *stabilization->rate_result =
                (uint32_t)RATE_CONTROLLER_RESULT_DISABLED;
        }
    }
}

static void reset_takeoff_leveling(
    flight_control_stabilization_t *stabilization)
{
    if ((stabilization != NULL) &&
        (stabilization->takeoff_leveling != NULL)) {
        takeoff_leveling_reset(stabilization->takeoff_leveling,
                               stabilization->easy_mode);
    }
}

static flight_control_result_t enter_control_failsafe(
    flight_control_stabilization_t *stabilization,
    flight_control_result_t accepted_result)
{
    reset_stabilization(stabilization);
    return motor_control_enter_failsafe() == MOTOR_CONTROL_FAILSAFE_ACCEPTED
               ? accepted_result
               : FLIGHT_CONTROL_FAILSAFE_ERROR;
}

static bool create_stage_one_setpoint(
    const prepared_control_input_shaping_t *control,
    const receiver_control_snapshot_t *requested,
    control_setpoint_t *setpoint)
{
    if ((control == NULL) || (requested == NULL) || !requested->valid ||
        (setpoint == NULL) || (requested->roll < -1.0F) ||
        (requested->roll > 1.0F) || (requested->pitch < -1.0F) ||
        (requested->pitch > 1.0F) || (requested->yaw < -1.0F) ||
        (requested->yaw > 1.0F) || (requested->throttle < 0.0F) ||
        (requested->throttle > 1.0F)) {
        return false;
    }
    *setpoint = (control_setpoint_t){
        .roll_normalized = requested->roll,
        .pitch_normalized = requested->pitch,
        .yaw_normalized = requested->yaw,
        .throttle = requested->throttle,
        .desired_roll_degrees =
            requested->roll * control->roll.maximum_angle_degrees,
        .desired_pitch_degrees =
            requested->pitch * control->pitch.maximum_angle_degrees,
        .desired_yaw_rate_dps =
            requested->yaw * control->yaw.maximum_rate_dps,
        .valid = true,
    };
    return true;
}

static control_objective_t create_control_objective(
    const control_setpoint_t *setpoint,
    float effective_roll_degrees,
    float effective_pitch_degrees,
    uint64_t now_us)
{
    return (control_objective_t){
        .axis_mode = {
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_RATE,
        },
        .axis_value = {
            effective_roll_degrees,
            effective_pitch_degrees,
            setpoint->desired_yaw_rate_dps,
        },
        .throttle = setpoint->throttle,
        .produced_at_us = now_us,
        .valid_until_us = now_us,
        .valid = setpoint->valid,
    };
}

static void publish_core_observation(
    flight_control_stabilization_t *stabilization,
    const flight_control_output_t *core_output)
{
    size_t axis;

    if (stabilization->desired_rates != NULL) {
        *stabilization->desired_rates = (flight_control_desired_rates_t){0};
        for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
            stabilization->desired_rates->desired_rate_dps[axis] =
                core_output->desired_rate_dps[axis];
        }
        stabilization->desired_rates->valid =
            core_output->rate_result == RATE_CONTROLLER_RESULT_SEEDED ||
            core_output->rate_result == RATE_CONTROLLER_RESULT_UPDATED;
    }
    if (stabilization->rate_output != NULL) {
        *stabilization->rate_output = core_output->rate_output;
    }
    if (stabilization->rate_result != NULL) {
        *stabilization->rate_result = (uint32_t)core_output->rate_result;
    }
}

flight_control_result_t flight_control_process_receiver(
    const prepared_control_input_shaping_t *control,
    const receiver_failsafe_decision_t *decision,
    flight_control_stabilization_t *stabilization,
    receiver_flight_control_output_t *output,
    uint64_t now_us)
{
    control_objective_t objective;
    flight_control_output_t core_output;
    flight_control_core_result_t core_result;
    control_setpoint_t setpoint;

    if (output != NULL) {
        *output = (receiver_flight_control_output_t){0};
    }
    if ((control == NULL) || (decision == NULL) ||
        (motor_control_active_source() != MOTOR_CONTROL_SOURCE_RECEIVER) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_NONE)) {
        reset_stabilization(stabilization);
        reset_takeoff_leveling(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STOP) {
        reset_stabilization(stabilization);
        reset_takeoff_leveling(stabilization);
        return motor_control_enter_failsafe() ==
                       MOTOR_CONTROL_FAILSAFE_ACCEPTED
                   ? FLIGHT_CONTROL_FAILSAFE_ENTERED
                   : FLIGHT_CONTROL_FAILSAFE_ERROR;
    }
    if ((decision->action == RECEIVER_FAILSAFE_ACTION_LIVE) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_HOLD_LAST)) {
        if (!control_input_shaping_apply(control,
                                         &decision->requested_control,
                                         &setpoint)) {
            return enter_control_failsafe(
                stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
        }
    } else if ((decision->action != RECEIVER_FAILSAFE_ACTION_STAGE_ONE) ||
               !create_stage_one_setpoint(control,
                                          &decision->requested_control,
                                          &setpoint)) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    if (output != NULL) {
        output->setpoint = setpoint;
    }
    if ((stabilization == NULL) ||
        !easy_mode_config_is_valid(stabilization->easy_mode) ||
        (stabilization->takeoff_leveling == NULL) ||
        (stabilization->core == NULL) ||
        (stabilization->profile == NULL) ||
        !stabilization->takeoff_leveling->initialized) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    if (setpoint.throttle == 0.0F) {
        if ((stabilization->attitude != NULL) &&
            stabilization->attitude->valid) {
            (void)takeoff_leveling_apply(
                stabilization->takeoff_leveling,
                stabilization->easy_mode,
                0.0F,
                setpoint.desired_roll_degrees,
                setpoint.desired_pitch_degrees,
                stabilization->attitude->roll_degrees,
                stabilization->attitude->pitch_degrees,
                stabilization->attitude->acquired_at_us,
                &stabilization->takeoff_leveling->effective_roll_degrees,
                &stabilization->takeoff_leveling->effective_pitch_degrees);
        }
        objective = create_control_objective(
            &setpoint,
            stabilization->takeoff_leveling->effective_roll_degrees,
            stabilization->takeoff_leveling->effective_pitch_degrees,
            now_us);
        core_result = flight_control_update(
            stabilization->core, stabilization->profile,
            stabilization->vehicle_state, &objective, now_us, &core_output);
        if (core_result != FLIGHT_CONTROL_CORE_UPDATED) {
            return enter_control_failsafe(
                stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
        }
        publish_core_observation(stabilization, &core_output);
        if (output != NULL) {
            output->mixer_output = core_output.mixer_output;
            output->mixer_output_valid = true;
            output->effective_roll_degrees =
                stabilization->takeoff_leveling->effective_roll_degrees;
            output->effective_pitch_degrees =
                stabilization->takeoff_leveling->effective_pitch_degrees;
            output->motor_baseline = core_output.motor_baseline;
            output->takeoff_leveling_state =
                stabilization->takeoff_leveling->state;
        }
        return motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                    &core_output.mixer_output.command) ==
                       MOTOR_CONTROL_SUBMIT_ACCEPTED
                   ? FLIGHT_CONTROL_SUBMITTED
                   : FLIGHT_CONTROL_SUBMIT_ERROR;
    }
    if (stabilization->imu_freshness == IMU_FRESHNESS_STALE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (stabilization->imu_freshness != IMU_FRESHNESS_FRESH) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_IMU_FAILSAFE_ENTERED);
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STAGE_ONE) {
        takeoff_leveling_complete(stabilization->takeoff_leveling,
                                  setpoint.desired_roll_degrees,
                                  setpoint.desired_pitch_degrees);
    } else if (!takeoff_leveling_apply(
                   stabilization->takeoff_leveling,
                   stabilization->easy_mode,
                   setpoint.throttle,
                   setpoint.desired_roll_degrees,
                   setpoint.desired_pitch_degrees,
                   stabilization->attitude->roll_degrees,
                   stabilization->attitude->pitch_degrees,
                   stabilization->attitude->acquired_at_us,
                   &stabilization->takeoff_leveling->effective_roll_degrees,
                   &stabilization->takeoff_leveling->effective_pitch_degrees)) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    objective = create_control_objective(
        &setpoint,
        stabilization->takeoff_leveling->effective_roll_degrees,
        stabilization->takeoff_leveling->effective_pitch_degrees,
        now_us);
    core_result = flight_control_update(
        stabilization->core, stabilization->profile,
        stabilization->vehicle_state, &objective, now_us, &core_output);
    if (core_result == FLIGHT_CONTROL_CORE_WAITING_FOR_STATE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (core_result != FLIGHT_CONTROL_CORE_UPDATED) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    publish_core_observation(stabilization, &core_output);
    if (core_output.rate_result == RATE_CONTROLLER_RESULT_SEEDED) {
        setpoint.throttle = 0.0F;
    }
    if (output != NULL) {
        output->setpoint = setpoint;
        output->mixer_output = core_output.mixer_output;
        output->mixer_output_valid = true;
        output->motor_baseline = core_output.motor_baseline;
        output->effective_roll_degrees =
            stabilization->takeoff_leveling->effective_roll_degrees;
        output->effective_pitch_degrees =
            stabilization->takeoff_leveling->effective_pitch_degrees;
        output->takeoff_leveling_state =
            stabilization->takeoff_leveling->state;
    }
    return motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                &core_output.mixer_output.command) ==
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
