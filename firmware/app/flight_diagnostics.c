#include "flight_diagnostics.h"

#include "application_state.h"

void flight_diagnostics_capture(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const flight_control_output_t *output,
    flight_control_result_t control_result)
{
    control_trace_sample_t sample;
    const bool usb_trace_enabled =
        firmware_control_trace.level != CONTROL_TRACE_LEVEL_OFF;
    const bool blackbox_interested =
        (firmware_blackbox.status == BLACKBOX_STATUS_RECORDING) ||
        (firmware_blackbox.status == BLACKBOX_STATUS_READY &&
         firmware_system_state_machine.current == SYSTEM_STATE_ARMED);

    if (!usb_trace_enabled && !blackbox_interested) {
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
    if (!imu_processing_pipeline_latest_observation(
            &firmware_imu_processing_pipeline, &sample.imu_observation) ||
        (sample.imu_observation.source_sequence !=
         sample.attitude.source_sequence)) {
        sample.imu_observation = (imu_processing_observation_t){0};
    }
    if (usb_trace_enabled) {
        (void)control_trace_record(&firmware_control_trace, &sample);
    }
    if (blackbox_interested) {
        blackbox_capture(&firmware_blackbox, &sample);
    }
}
