#include "manual_easy_behavior.h"

#include <stddef.h>

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

static control_objective_t create_objective(
    const manual_easy_behavior_t *behavior,
    uint64_t now_us)
{
    return (control_objective_t){
        .axis_mode = {
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_ANGLE,
            CONTROL_OBJECTIVE_AXIS_RATE,
        },
        .axis_value = {
            behavior->takeoff_leveling.effective_roll_degrees,
            behavior->takeoff_leveling.effective_pitch_degrees,
            behavior->last_setpoint.desired_yaw_rate_dps,
        },
        .throttle = behavior->last_setpoint.throttle,
        .produced_at_us = now_us,
        .valid_until_us = now_us,
        .valid = true,
    };
}

bool manual_easy_behavior_initialize(manual_easy_behavior_t *behavior,
                                     const easy_mode_config_t *easy_mode)
{
    if ((behavior == NULL) || !easy_mode_config_is_valid(easy_mode)) {
        return false;
    }
    *behavior = (manual_easy_behavior_t){0};
    takeoff_leveling_initialize(&behavior->takeoff_leveling);
    takeoff_leveling_reset(&behavior->takeoff_leveling, easy_mode);
    behavior->initialized = true;
    return true;
}

void manual_easy_behavior_reset(manual_easy_behavior_t *behavior,
                                const easy_mode_config_t *easy_mode)
{
    if ((behavior == NULL) || !behavior->initialized ||
        !easy_mode_config_is_valid(easy_mode)) {
        return;
    }
    behavior->last_setpoint = (control_setpoint_t){0};
    takeoff_leveling_reset(&behavior->takeoff_leveling, easy_mode);
}

static bool prepare_setpoint(
    const prepared_control_input_shaping_t *control,
    const receiver_failsafe_decision_t *decision,
    control_setpoint_t *setpoint)
{
    if ((decision->action == RECEIVER_FAILSAFE_ACTION_LIVE) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_HOLD_LAST)) {
        return control_input_shaping_apply(
            control, &decision->requested_control, setpoint);
    }
    return (decision->action == RECEIVER_FAILSAFE_ACTION_STAGE_ONE) &&
           create_stage_one_setpoint(
               control, &decision->requested_control, setpoint);
}

flight_behavior_result_t manual_easy_behavior_update(
    manual_easy_behavior_t *behavior,
    const prepared_control_input_shaping_t *control,
    const easy_mode_config_t *easy_mode,
    const receiver_failsafe_decision_t *decision,
    const vehicle_state_t *vehicle_state,
    imu_freshness_t imu_freshness,
    uint64_t now_us,
    control_objective_t *objective)
{
    if (objective != NULL) {
        *objective = (control_objective_t){0};
    }
    if ((behavior == NULL) || !behavior->initialized) {
        return FLIGHT_BEHAVIOR_NOT_INITIALIZED;
    }
    if ((control == NULL) || !control->initialized ||
        !easy_mode_config_is_valid(easy_mode) || (decision == NULL) ||
        (objective == NULL)) {
        return FLIGHT_BEHAVIOR_INVALID_CONTROL;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_NONE) {
        manual_easy_behavior_reset(behavior, easy_mode);
        return FLIGHT_BEHAVIOR_INACTIVE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STOP) {
        manual_easy_behavior_reset(behavior, easy_mode);
        return FLIGHT_BEHAVIOR_STOP_REQUESTED;
    }
    if (!prepare_setpoint(control, decision, &behavior->last_setpoint)) {
        return FLIGHT_BEHAVIOR_INVALID_CONTROL;
    }
    if (behavior->last_setpoint.throttle == 0.0F) {
        if ((vehicle_state != NULL) && vehicle_state->valid) {
            (void)takeoff_leveling_apply(
                &behavior->takeoff_leveling,
                easy_mode,
                0.0F,
                behavior->last_setpoint.desired_roll_degrees,
                behavior->last_setpoint.desired_pitch_degrees,
                vehicle_state->attitude_degrees[RATE_CONTROLLER_AXIS_ROLL],
                vehicle_state->attitude_degrees[RATE_CONTROLLER_AXIS_PITCH],
                vehicle_state->acquired_at_us,
                &behavior->takeoff_leveling.effective_roll_degrees,
                &behavior->takeoff_leveling.effective_pitch_degrees);
        }
        *objective = create_objective(behavior, now_us);
        return FLIGHT_BEHAVIOR_OBJECTIVE_READY;
    }
    if (imu_freshness == IMU_FRESHNESS_STALE) {
        return FLIGHT_BEHAVIOR_WAITING_FOR_STATE;
    }
    if ((imu_freshness != IMU_FRESHNESS_FRESH) ||
        (vehicle_state == NULL) || !vehicle_state->valid) {
        return FLIGHT_BEHAVIOR_INVALID_STATE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STAGE_ONE) {
        takeoff_leveling_complete(
            &behavior->takeoff_leveling,
            behavior->last_setpoint.desired_roll_degrees,
            behavior->last_setpoint.desired_pitch_degrees);
    } else if (!takeoff_leveling_apply(
                   &behavior->takeoff_leveling,
                   easy_mode,
                   behavior->last_setpoint.throttle,
                   behavior->last_setpoint.desired_roll_degrees,
                   behavior->last_setpoint.desired_pitch_degrees,
                   vehicle_state
                       ->attitude_degrees[RATE_CONTROLLER_AXIS_ROLL],
                   vehicle_state
                       ->attitude_degrees[RATE_CONTROLLER_AXIS_PITCH],
                   vehicle_state->acquired_at_us,
                   &behavior->takeoff_leveling.effective_roll_degrees,
                   &behavior->takeoff_leveling.effective_pitch_degrees)) {
        return FLIGHT_BEHAVIOR_INVALID_CONTROL;
    }
    *objective = create_objective(behavior, now_us);
    return FLIGHT_BEHAVIOR_OBJECTIVE_READY;
}
