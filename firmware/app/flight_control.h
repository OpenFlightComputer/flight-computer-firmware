#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H

#include "flight_control_core.h"
#include "motor_control.h"
#include "receiver_failsafe.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FLIGHT_CONTROL_IDLE = 0,
    FLIGHT_CONTROL_SUBMITTED,
    FLIGHT_CONTROL_FAILSAFE_ENTERED,
    FLIGHT_CONTROL_RECOVERED,
    FLIGHT_CONTROL_WAITING_FOR_IMU,
    FLIGHT_CONTROL_IMU_FAILSAFE_ENTERED,
    FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED,
    FLIGHT_CONTROL_SUBMIT_ERROR,
    FLIGHT_CONTROL_FAILSAFE_ERROR,
    FLIGHT_CONTROL_RECOVERY_ERROR,
} flight_control_result_t;

typedef struct {
    float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT];
    bool valid;
} flight_control_desired_rates_t;

typedef struct {
    flight_control_core_t *core;
    const prepared_control_profile_t *profile;
    const vehicle_state_t *vehicle_state;
    flight_control_desired_rates_t *desired_rates;
    rate_controller_output_t *rate_output;
    volatile uint32_t *rate_result;
} flight_control_stabilization_t;

void flight_control_reset(flight_control_stabilization_t *stabilization);
flight_control_result_t flight_control_enter_failsafe(
    flight_control_stabilization_t *stabilization,
    flight_control_result_t accepted_result);
flight_control_result_t flight_control_execute_objective(
    motor_control_source_t authority,
    const control_objective_t *objective,
    flight_control_stabilization_t *stabilization,
    flight_control_output_t *output,
    uint64_t now_us);
flight_control_result_t flight_control_recover_receiver(
    receiver_failsafe_t *failsafe);

#endif
