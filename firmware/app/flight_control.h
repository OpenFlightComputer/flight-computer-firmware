#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H

#include "flight_configuration.h"
#include "control_input_shaping.h"
#include "imu_processing_pipeline.h"
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
    const roll_attitude_controller_config_t *roll_controller;
    const pitch_attitude_controller_config_t *pitch_controller;
    rate_controller_t *rate_controller;
    const attitude_snapshot_t *attitude;
    imu_freshness_t imu_freshness;
    flight_control_desired_rates_t *desired_rates;
    rate_controller_output_t *rate_output;
    volatile uint32_t *rate_result;
} flight_control_stabilization_t;

flight_control_result_t flight_control_process_receiver(
    const prepared_control_input_shaping_t *control,
    const prepared_quad_x_mixer_t *mixer,
    const receiver_failsafe_decision_t *decision,
    flight_control_stabilization_t *stabilization,
    uint64_t now_us);
flight_control_result_t flight_control_recover_receiver(
    receiver_failsafe_t *failsafe);

#endif
