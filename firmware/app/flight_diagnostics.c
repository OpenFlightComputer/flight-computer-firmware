#include "flight_diagnostics.h"

#include "application_state.h"

#define BLACKBOX_USB_ENUMERATION_GRACE_US UINT64_C(2000000)

void flight_diagnostics_capture(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const flight_diagnostic_control_t *output,
    flight_control_result_t control_result)
{
    control_trace_sample_t sample;
    const bool usb_trace_enabled =
        firmware_control_trace.level != CONTROL_TRACE_LEVEL_OFF;
    const bool blackbox_start_allowed =
        (now_us >= BLACKBOX_USB_ENUMERATION_GRACE_US) ||
        (firmware_system_state_machine.current == SYSTEM_STATE_ARMED) ||
        (firmware_system_state_machine.current == SYSTEM_STATE_FAILSAFE) ||
        (firmware_system_state_machine.current == SYSTEM_STATE_FAULT);
    const bool blackbox_must_finish =
        firmware_usb_connected &&
        (firmware_blackbox.status == BLACKBOX_STATUS_RECORDING);
    const bool blackbox_interested =
        !firmware_usb_connected && blackbox_start_allowed &&
        blackbox_capture_due(&firmware_blackbox,
                             now_us,
                             firmware_system_state_machine.current);

    if (!usb_trace_enabled && !blackbox_interested && !blackbox_must_finish) {
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
        .mixer_output = output->control.mixer_output,
        .effective_attitude_target_degrees = {
            output->effective_roll_degrees,
            output->effective_pitch_degrees,
        },
        .motor_baseline = output->control.motor_baseline,
        .takeoff_leveling_state = output->takeoff_leveling_state,
        .level_calibration_state =
            (uint8_t)firmware_level_calibration.state,
        .mixer_output_valid = output->control.mixer_output_valid,
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
    if (blackbox_must_finish) {
        blackbox_finish_recording(&firmware_blackbox, &sample);
    } else if (blackbox_interested) {
        blackbox_capture(&firmware_blackbox, &sample);
    }
}
