#ifndef OPENFLIGHTCOMPUTER_FLIGHT_DIAGNOSTICS_H
#define OPENFLIGHTCOMPUTER_FLIGHT_DIAGNOSTICS_H

#include "control_input_shaping.h"
#include "flight_control.h"
#include "imu_processing_pipeline.h"
#include "takeoff_leveling.h"

typedef struct {
    control_setpoint_t setpoint;
    flight_control_output_t control;
    float effective_roll_degrees;
    float effective_pitch_degrees;
    takeoff_leveling_state_t takeoff_leveling_state;
} flight_diagnostic_control_t;

void flight_diagnostics_capture(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const flight_diagnostic_control_t *output,
    flight_control_result_t control_result);

#endif
