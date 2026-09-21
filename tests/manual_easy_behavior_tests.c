#include "manual_easy_behavior.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
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
        .roll = {.maximum_angle_degrees = 30.0F,
                 .maximum_rate_dps = 180.0F, .curve = identity},
        .pitch = {.maximum_angle_degrees = 30.0F,
                  .maximum_rate_dps = 180.0F, .curve = identity},
        .yaw = {.maximum_rate_dps = 150.0F, .curve = identity},
        .throttle = {.maximum = 1.0F, .curve = identity},
    };
}

static receiver_failsafe_decision_t live_decision(void)
{
    return (receiver_failsafe_decision_t){
        .action = RECEIVER_FAILSAFE_ACTION_LIVE,
        .requested_control = {
            .roll = 0.4F, .pitch = -0.2F, .yaw = 0.5F,
            .throttle = 0.5F, .valid = true,
        },
    };
}

int main(void)
{
    const control_input_shaping_config_t config = control_configuration();
    const easy_mode_config_t easy_mode = {
        .armed_idle_permille = 50U,
        .activation_throttle_permille = 180U,
        .leveling_rate_decidegrees_per_second = 100U,
        .takeoff_leveling_enabled = true,
    };
    prepared_control_input_shaping_t control;
    manual_easy_behavior_t behavior;
    receiver_failsafe_decision_t decision = live_decision();
    vehicle_state_t state = {
        .attitude_degrees = {5.0F, -5.0F, 0.0F},
        .angular_rate_dps = {10.0F, -5.0F, 2.0F},
        .acquired_at_us = 1000U,
        .source_sequence = 1U,
        .valid = true,
    };
    control_objective_t objective;

    assert(control_input_shaping_prepare(&config, &control));
    assert(manual_easy_behavior_initialize(&behavior, &easy_mode));
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_FRESH, 1000U, &objective) ==
           FLIGHT_BEHAVIOR_OBJECTIVE_READY);
    assert(close_to(objective.axis_value[RATE_CONTROLLER_AXIS_ROLL], 5.0F));
    assert(close_to(objective.axis_value[RATE_CONTROLLER_AXIS_PITCH], -5.0F));
    assert(close_to(objective.axis_value[RATE_CONTROLLER_AXIS_YAW], 75.0F));
    assert(close_to(objective.throttle, 0.5F));

    decision.requested_control.throttle = 0.0F;
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, NULL,
               IMU_FRESHNESS_UNAVAILABLE, 2000U, &objective) ==
           FLIGHT_BEHAVIOR_OBJECTIVE_READY);
    assert(objective.throttle == 0.0F);

    decision = live_decision();
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_STALE, 3000U, &objective) ==
           FLIGHT_BEHAVIOR_WAITING_FOR_STATE);
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_LOST, 3000U, &objective) ==
           FLIGHT_BEHAVIOR_INVALID_STATE);

    decision.action = RECEIVER_FAILSAFE_ACTION_STAGE_ONE;
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_FRESH, 4000U, &objective) ==
           FLIGHT_BEHAVIOR_OBJECTIVE_READY);
    assert(close_to(objective.axis_value[RATE_CONTROLLER_AXIS_ROLL], 12.0F));
    assert(close_to(objective.axis_value[RATE_CONTROLLER_AXIS_PITCH], -6.0F));

    decision.action = RECEIVER_FAILSAFE_ACTION_STOP;
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_FRESH, 5000U, &objective) ==
           FLIGHT_BEHAVIOR_STOP_REQUESTED);
    decision.action = RECEIVER_FAILSAFE_ACTION_NONE;
    assert(manual_easy_behavior_update(
               &behavior, &control, &easy_mode, &decision, &state,
               IMU_FRESHNESS_FRESH, 6000U, &objective) ==
           FLIGHT_BEHAVIOR_INACTIVE);
    return 0;
}
