#include "control_trace_recorder.h"

#include "application_state.h"

void control_trace_recorder_record(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const flight_control_output_t *output,
    flight_control_result_t control_result)
{
    control_trace_sample_t sample;

    if (firmware_control_trace.level == CONTROL_TRACE_LEVEL_OFF) {
        return;
    }
    sample = (control_trace_sample_t){
        .timestamp_us = now_us,
        .system_state = firmware_system_state_machine.current,
        .control_source = motor_control_active_source(),
        .failsafe_state = decision->state,
        .failsafe_action = decision->action,
        .imu_freshness = (imu_freshness_t)firmware_imu_freshness,
        .control_result = control_result,
        .rate_result = (rate_controller_result_t)firmware_rate_controller_result,
        .receiver = decision->requested_control,
        .setpoint = output->setpoint,
        .attitude = *attitude,
        .desired_rates = firmware_flight_control_desired_rates,
        .rate_output = firmware_rate_controller_output,
        .mixer_output = output->mixer_output,
        .mixer_output_valid = output->mixer_output_valid,
    };
    (void)control_trace_record(&firmware_control_trace, &sample);
}
