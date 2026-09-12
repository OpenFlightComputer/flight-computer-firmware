#include "usb_command_processor.h"

#include "usb_control_trace_response.h"
#include "health.h"
#include "logging.h"
#include "motor_control.h"
#include "motor_safety_policy.h"
#include "usb_health_response.h"
#include "usb_imu_response.h"
#include "usb_json_protocol.h"
#include "usb_receiver_response.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define USB_MOTOR_TEST_THROTTLE_SCALE 1000000.0f

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static usb_command_process_result_t try_send_pending_response(
    usb_command_processor_t *processor)
{
    usb_cdc_write_result_t write_result;

    write_result = usb_cdc_transport_try_write(
        (const uint8_t *)processor->pending_response,
        processor->pending_response_length);
    switch (write_result) {
    case USB_CDC_WRITE_ACCEPTED:
        if (processor->pending_trace_discard_count > 0U) {
            control_trace_discard(processor->control_trace,
                                  processor->pending_trace_discard_count);
            processor->pending_trace_discard_count = 0U;
        }
        processor->pending_response_valid = false;
        processor->pending_response_length = 0U;
        saturating_increment(&processor->statistics.response_sent_count);
        return USB_COMMAND_PROCESS_RESPONSE_SENT;
    case USB_CDC_WRITE_BUSY:
        saturating_increment(&processor->statistics.response_busy_count);
        return USB_COMMAND_PROCESS_RESPONSE_PENDING;
    case USB_CDC_WRITE_ERROR:
        processor->pending_trace_discard_count = 0U;
        processor->pending_response_valid = false;
        processor->pending_response_length = 0U;
        saturating_increment(&processor->statistics.response_error_count);
        return USB_COMMAND_PROCESS_TRANSPORT_ERROR;
    }

    processor->pending_trace_discard_count = 0U;
    processor->pending_response_valid = false;
    processor->pending_response_length = 0U;
    saturating_increment(&processor->statistics.response_error_count);
    return USB_COMMAND_PROCESS_TRANSPORT_ERROR;
}

static control_trace_level_t control_trace_level_from_usb(
    usb_json_trace_level_t level)
{
    switch (level) {
    case USB_JSON_TRACE_LEVEL_EVENTS:
        return CONTROL_TRACE_LEVEL_EVENTS;
    case USB_JSON_TRACE_LEVEL_LOW_RATE:
        return CONTROL_TRACE_LEVEL_LOW_RATE;
    case USB_JSON_TRACE_LEVEL_HIGH_RATE:
        return CONTROL_TRACE_LEVEL_HIGH_RATE;
    case USB_JSON_TRACE_LEVEL_FULL_RATE:
        return CONTROL_TRACE_LEVEL_FULL_RATE;
    case USB_JSON_TRACE_LEVEL_COUNT:
        break;
    }

    return CONTROL_TRACE_LEVEL_OFF;
}

static bool build_control_trace_start_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    const control_trace_level_t level =
        control_trace_level_from_usb(request->trace_level);
    const bool state_allows_start =
        processor->state_machine->current == SYSTEM_STATE_DISARMED;
    const bool accepted = state_allows_start &&
                          control_trace_start(processor->control_trace,
                                              level,
                                              processor->clock());
    const char *error = NULL;

    saturating_increment(&processor->statistics.control_trace_start_count);
    if (!accepted) {
        saturating_increment(
            &processor->statistics.control_trace_rejected_count);
        error = state_allows_start ? "trace_already_active"
                                   : "state_rejected";
    }
    return usb_control_trace_status_response_build(
        "control_trace_start",
        request->request_id,
        accepted,
        processor->state_machine->current,
        error,
        processor->control_trace,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static bool build_control_trace_stop_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    saturating_increment(&processor->statistics.control_trace_stop_count);
    control_trace_stop(processor->control_trace);
    return usb_control_trace_status_response_build(
        "control_trace_stop",
        request->request_id,
        true,
        processor->state_machine->current,
        NULL,
        processor->control_trace,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static bool build_control_trace_read_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    control_trace_batch_t batch;
    control_trace_record_t records[CONTROL_TRACE_USB_RECORD_LIMIT];
    size_t serialized_count = 0U;

    saturating_increment(&processor->statistics.control_trace_read_count);
    if (!control_trace_peek(processor->control_trace,
                            records,
                            CONTROL_TRACE_USB_RECORD_LIMIT,
                            &batch)) {
        return false;
    }
    if (!usb_control_trace_read_response_build(
            request->request_id,
            processor->control_trace,
            records,
            &batch,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length,
            &serialized_count)) {
        return false;
    }
    processor->pending_trace_discard_count = serialized_count;
    return true;
}

static bool build_control_trace_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    switch (request->command) {
    case USB_JSON_COMMAND_CONTROL_TRACE_START:
        return build_control_trace_start_response(processor, request);
    case USB_JSON_COMMAND_CONTROL_TRACE_STOP:
        return build_control_trace_stop_response(processor, request);
    case USB_JSON_COMMAND_CONTROL_TRACE_READ:
        return build_control_trace_read_response(processor, request);
    default:
        return false;
    }
}

static bool build_error(usb_command_processor_t *processor,
                        bool request_id_valid,
                        uint32_t request_id,
                        const char *error)
{
    return usb_json_build_error_response(
        request_id_valid,
        request_id,
        error,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static const char *motor_test_error(motor_control_submit_result_t result)
{
    switch (result) {
    case MOTOR_CONTROL_SUBMIT_BLOCKED_STATE:
        return "state_rejected";
    case MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE:
        return "control_source_rejected";
    case MOTOR_CONTROL_SUBMIT_BLOCKED_PREPARATION:
        return "motor_not_ready";
    case MOTOR_CONTROL_SUBMIT_BLOCKED_HEALTH:
        return "health_rejected";
    case MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED:
    case MOTOR_CONTROL_SUBMIT_INVALID_COMMAND:
    case MOTOR_CONTROL_SUBMIT_STALE_COMMAND:
    case MOTOR_CONTROL_SUBMIT_MAPPING_ERROR:
    case MOTOR_CONTROL_SUBMIT_FORCE_STOP_ERROR:
        return "motor_output_error";
    case MOTOR_CONTROL_SUBMIT_ACCEPTED:
        break;
    }

    return "motor_output_error";
}

static bool build_motor_test_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    float throttles[MOTOR_COMMAND_MOTOR_COUNT] = {0.0f};
    motor_command_t command;
    motor_control_submit_result_t result;

    saturating_increment(&processor->statistics.motor_test_count);
    if ((request->motor == 0U) ||
        (request->motor > MOTOR_COMMAND_MOTOR_COUNT)) {
        saturating_increment(&processor->statistics.motor_test_rejected_count);
        return usb_json_build_motor_test_response(
            request->request_id,
            false,
            request->motor,
            request->throttle_millionths,
            system_state_name(processor->state_machine->current),
            "motor_not_allowed",
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    throttles[request->motor - 1U] =
        (float)request->throttle_millionths /
        USB_MOTOR_TEST_THROTTLE_SCALE;
    if (motor_command_create(&command, throttles, processor->clock()) !=
        MOTOR_COMMAND_CREATE_OK) {
        result = MOTOR_CONTROL_SUBMIT_INVALID_COMMAND;
    } else {
        result = motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST,
                                      &command);
    }

    if (result == MOTOR_CONTROL_SUBMIT_ACCEPTED) {
        saturating_increment(&processor->statistics.motor_test_accepted_count);
    } else {
        saturating_increment(&processor->statistics.motor_test_rejected_count);
    }
    return usb_json_build_motor_test_response(
        request->request_id,
        result == MOTOR_CONTROL_SUBMIT_ACCEPTED,
        request->motor,
        request->throttle_millionths,
        system_state_name(processor->state_machine->current),
        result == MOTOR_CONTROL_SUBMIT_ACCEPTED ? NULL
                                               : motor_test_error(result),
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static void build_imu_diagnostics(
    const usb_command_processor_t *processor,
    usb_imu_diagnostics_t *diagnostics)
{
    uint64_t high_rate_budget_us = 0U;
    size_t index;

    *diagnostics = (usb_imu_diagnostics_t){
        .service_statistics = processor->imu_service->statistics,
    };
    if (!imu_service_state(processor->imu_service, &diagnostics->state)) {
        diagnostics->state = (imu_service_state_t){
            .freshness = IMU_FRESHNESS_UNAVAILABLE,
            .age_us = UINT64_MAX,
        };
    }
    diagnostics->calibration_state = processor->gyro_calibration->state;
    diagnostics->calibration_sample_count =
        processor->gyro_calibration->sample_count;
    diagnostics->calibration_restart_count =
        processor->gyro_calibration->restart_count;
    diagnostics->calibration_progress_permille =
        gyro_calibration_progress_permille(processor->gyro_calibration,
                                           processor->clock());
    diagnostics->calibration_ready = gyro_calibration_bias(
        processor->gyro_calibration, diagnostics->calibration_bias);
    diagnostics->processing_statistics =
        processor->imu_processing_pipeline->statistics;
    diagnostics->attitude = processor->imu_processing_pipeline->latest;
    if (diagnostics->calibration_ready) {
        (void)gyro_calibration_correct(processor->gyro_calibration,
                                       &diagnostics->state.snapshot,
                                       diagnostics->corrected_gyroscope);
    }

    for (index = 0U;
         index < task_registry_count(processor->task_registry);
         index++) {
        const task_t *task = task_registry_task_at(processor->task_registry,
                                                   index);

        if (task == NULL) {
            continue;
        }
        if (task->enabled && (task->definition.period_us == 1000U)) {
            high_rate_budget_us += task->maximum_execution_time_us;
        }
        if ((task->definition.name != NULL) &&
            (strcmp(task->definition.name, "imu-service") == 0)) {
            diagnostics->task_execution_count = task->execution_count;
            diagnostics->task_last_execution_us =
                task->last_execution_time_us;
            diagnostics->task_maximum_execution_us =
                task->maximum_execution_time_us;
            diagnostics->task_overrun_count = task->overrun_count;
            diagnostics->task_missed_release_count =
                task->missed_release_count;
            diagnostics->task_present = true;
        }
    }
    diagnostics->high_rate_budget_us =
        high_rate_budget_us > UINT32_MAX ? UINT32_MAX
                                         : (uint32_t)high_rate_budget_us;
    diagnostics->high_rate_utilization_permille =
        high_rate_budget_us > (UINT32_MAX / UINT32_C(1000))
            ? UINT32_MAX
            : (uint32_t)((high_rate_budget_us * UINT32_C(1000)) /
                         UINT32_C(1000));
}

static void configuration_to_usb(
    const flight_configuration_t *configuration,
    usb_json_configuration_t *usb)
{
    const control_curve_config_t *curves[4] = {
        &configuration->control.roll.curve,
        &configuration->control.pitch.curve,
        &configuration->control.yaw.curve,
        &configuration->control.throttle.curve,
    };
    size_t curve;
    size_t point;
    size_t motor;

    *usb = (usb_json_configuration_t){
        .schema_version = configuration->schema_version,
        .timing_us = {
            configuration->receiver_failsafe.stale_after_us,
            configuration->receiver_failsafe.loss_detected_after_us,
            configuration->receiver_failsafe.hold_last_until_us,
            configuration->receiver_failsafe.stage_two_after_us,
            configuration->receiver_failsafe.recovery_stable_us,
        },
        .failsafe_control_millionths = {
            (int32_t)((configuration->receiver_failsafe.stage_one_roll *
                       1000000.0F) +
                      (configuration->receiver_failsafe.stage_one_roll >= 0.0F
                           ? 0.5F
                           : -0.5F)),
            (int32_t)((configuration->receiver_failsafe.stage_one_pitch *
                       1000000.0F) +
                      (configuration->receiver_failsafe.stage_one_pitch >= 0.0F
                           ? 0.5F
                           : -0.5F)),
            (int32_t)((configuration->receiver_failsafe.stage_one_yaw *
                       1000000.0F) +
                      (configuration->receiver_failsafe.stage_one_yaw >= 0.0F
                           ? 0.5F
                           : -0.5F)),
            (int32_t)((configuration->receiver_failsafe.stage_one_throttle *
                       1000000.0F) + 0.5F),
            (int32_t)((configuration->receiver_failsafe
                           .recovery_throttle_maximum * 1000000.0F) + 0.5F),
        },
        .propeller_layout = (uint8_t)configuration->propeller_layout,
        .gyro_timing_us = {
            configuration->gyro_calibration.settling_duration_us,
            configuration->gyro_calibration.sample_duration_us,
        },
        .gyro_threshold_millionths = {
            (uint32_t)((configuration->gyro_calibration.maximum_rate_dps *
                        1000000.0F) + 0.5F),
            (uint32_t)((configuration->gyro_calibration
                            .maximum_standard_deviation_dps * 1000000.0F) +
                       0.5F),
        },
        .gyro_filter_cutoff_millionths =
            (uint32_t)((configuration->gyro_filter.cutoff_hz * 1000000.0F) +
                       0.5F),
        .accelerometer_correction_time_constant_millionths =
            (uint32_t)((configuration->attitude_estimator
                            .accelerometer_correction_time_constant_s *
                        1000000.0F) + 0.5F),
        .attitude_maximum_gap_us =
            configuration->attitude_estimator.maximum_gap_us,
        .gyro_filter_type = (uint8_t)configuration->gyro_filter.type,
        .attitude_estimator_type =
            (uint8_t)configuration->attitude_estimator.type,
        .control_axis_millionths = {
            {(uint32_t)(configuration->control.roll.deadband * 1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.roll.maximum_angle_degrees *
                            1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.roll.maximum_rate_dps *
                            1000000.0F +
                        0.5F)},
            {(uint32_t)(configuration->control.pitch.deadband * 1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.pitch.maximum_angle_degrees *
                            1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.pitch.maximum_rate_dps *
                            1000000.0F +
                        0.5F)},
            {(uint32_t)(configuration->control.yaw.deadband * 1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.yaw.maximum_angle_degrees *
                            1000000.0F +
                        0.5F),
             (uint32_t)(configuration->control.yaw.maximum_rate_dps *
                            1000000.0F +
                        0.5F)},
        },
        .throttle_millionths = {
            (uint32_t)(configuration->control.throttle.zero_deadband *
                           1000000.0F +
                       0.5F),
            (uint32_t)(configuration->control.throttle.maximum * 1000000.0F +
                       0.5F),
        },
        .rate_controller_maximum_gap_us =
            configuration->rate_controller.maximum_gap_us,
        .rate_controller_type =
            (uint8_t)configuration->rate_controller.type,
        .attitude_gain_millionths = {
            (uint32_t)(configuration->roll_attitude_controller.gain_per_s *
                           1000000.0F +
                       0.5F),
            (uint32_t)(configuration->pitch_attitude_controller.gain_per_s *
                           1000000.0F +
                       0.5F),
        },
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        usb->directions[motor] =
            (uint8_t)configuration->motors.direction[motor];
    }
    for (curve = 0U; curve < RATE_CONTROLLER_AXIS_COUNT; curve++) {
        usb->rate_pid_millionths[curve][0] =
            (uint32_t)(configuration->rate_controller.axis[curve].kp *
                       1000000.0F + 0.5F);
        usb->rate_pid_millionths[curve][1] =
            (uint32_t)(configuration->rate_controller.axis[curve].ki *
                       1000000.0F + 0.5F);
        usb->rate_pid_millionths[curve][2] =
            (uint32_t)(configuration->rate_controller.axis[curve].kd *
                       1000000.0F + 0.5F);
        usb->rate_pid_millionths[curve][3] =
            (uint32_t)(configuration->rate_controller.axis[curve]
                           .integral_limit * 1000000.0F + 0.5F);
        usb->rate_pid_millionths[curve][4] =
            (uint32_t)(configuration->rate_controller.axis[curve]
                           .output_limit * 1000000.0F + 0.5F);
    }
    for (curve = 0U; curve < 4U; curve++) {
        usb->curve_type[curve] = (uint8_t)curves[curve]->type;
        usb->curve_interpolation[curve] =
            (uint8_t)curves[curve]->interpolation;
        usb->curve_point_count[curve] = curves[curve]->point_count;
        for (point = 0U; point < curves[curve]->point_count; point++) {
            usb->curve_point_millionths[curve][point][0] =
                (uint32_t)(curves[curve]->points[point].input * 1000000.0F +
                           0.5F);
            usb->curve_point_millionths[curve][point][1] =
                (uint32_t)(curves[curve]->points[point].output * 1000000.0F +
                           0.5F);
        }
    }
}

static void configuration_from_usb(
    const usb_json_configuration_t *usb,
    flight_configuration_t *configuration)
{
    control_curve_config_t *curves[4];
    size_t curve;
    size_t point;
    size_t motor;

    *configuration = (flight_configuration_t){
        .schema_version = usb->schema_version,
        .propeller_layout = (propeller_layout_t)usb->propeller_layout,
        .receiver_failsafe = {
            .stale_after_us = usb->timing_us[0],
            .loss_detected_after_us = usb->timing_us[1],
            .hold_last_until_us = usb->timing_us[2],
            .stage_two_after_us = usb->timing_us[3],
            .recovery_stable_us = usb->timing_us[4],
            .stage_one_roll =
                (float)usb->failsafe_control_millionths[0] / 1000000.0F,
            .stage_one_pitch =
                (float)usb->failsafe_control_millionths[1] / 1000000.0F,
            .stage_one_yaw =
                (float)usb->failsafe_control_millionths[2] / 1000000.0F,
            .stage_one_throttle =
                (float)usb->failsafe_control_millionths[3] / 1000000.0F,
            .recovery_throttle_maximum =
                (float)usb->failsafe_control_millionths[4] / 1000000.0F,
        },
        .gyro_calibration = {
            .settling_duration_us = usb->gyro_timing_us[0],
            .sample_duration_us = usb->gyro_timing_us[1],
            .maximum_rate_dps =
                (float)usb->gyro_threshold_millionths[0] / 1000000.0F,
            .maximum_standard_deviation_dps =
                (float)usb->gyro_threshold_millionths[1] / 1000000.0F,
        },
        .gyro_filter = {
            .type = (flight_gyro_filter_type_t)usb->gyro_filter_type,
            .cutoff_hz =
                (float)usb->gyro_filter_cutoff_millionths / 1000000.0F,
        },
        .attitude_estimator = {
            .type = (flight_attitude_estimator_type_t)
                usb->attitude_estimator_type,
            .accelerometer_correction_time_constant_s =
                (float)usb
                    ->accelerometer_correction_time_constant_millionths /
                1000000.0F,
            .maximum_gap_us = usb->attitude_maximum_gap_us,
        },
        .control = {
            .roll = {
                .deadband =
                    (float)usb->control_axis_millionths[0][0] / 1000000.0F,
                .maximum_angle_degrees =
                    (float)usb->control_axis_millionths[0][1] / 1000000.0F,
                .maximum_rate_dps =
                    (float)usb->control_axis_millionths[0][2] / 1000000.0F,
            },
            .pitch = {
                .deadband =
                    (float)usb->control_axis_millionths[1][0] / 1000000.0F,
                .maximum_angle_degrees =
                    (float)usb->control_axis_millionths[1][1] / 1000000.0F,
                .maximum_rate_dps =
                    (float)usb->control_axis_millionths[1][2] / 1000000.0F,
            },
            .yaw = {
                .deadband =
                    (float)usb->control_axis_millionths[2][0] / 1000000.0F,
                .maximum_angle_degrees =
                    (float)usb->control_axis_millionths[2][1] / 1000000.0F,
                .maximum_rate_dps =
                    (float)usb->control_axis_millionths[2][2] / 1000000.0F,
            },
            .throttle = {
                .zero_deadband =
                    (float)usb->throttle_millionths[0] / 1000000.0F,
                .maximum =
                    (float)usb->throttle_millionths[1] / 1000000.0F,
            },
        },
        .rate_controller = {
            .type = (rate_controller_type_t)usb->rate_controller_type,
            .maximum_gap_us = usb->rate_controller_maximum_gap_us,
        },
        .roll_attitude_controller = {
            .gain_per_s =
                (float)usb->attitude_gain_millionths[0] / 1000000.0F,
        },
        .pitch_attitude_controller = {
            .gain_per_s =
                (float)usb->attitude_gain_millionths[1] / 1000000.0F,
        },
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] =
            (motor_direction_t)usb->directions[motor];
    }
    for (curve = 0U; curve < RATE_CONTROLLER_AXIS_COUNT; curve++) {
        configuration->rate_controller.axis[curve] = (rate_pid_config_t){
            .kp = (float)usb->rate_pid_millionths[curve][0] / 1000000.0F,
            .ki = (float)usb->rate_pid_millionths[curve][1] / 1000000.0F,
            .kd = (float)usb->rate_pid_millionths[curve][2] / 1000000.0F,
            .integral_limit =
                (float)usb->rate_pid_millionths[curve][3] / 1000000.0F,
            .output_limit =
                (float)usb->rate_pid_millionths[curve][4] / 1000000.0F,
        };
    }
    curves[0] = &configuration->control.roll.curve;
    curves[1] = &configuration->control.pitch.curve;
    curves[2] = &configuration->control.yaw.curve;
    curves[3] = &configuration->control.throttle.curve;
    for (curve = 0U; curve < 4U; curve++) {
        curves[curve]->type =
            (control_curve_type_t)usb->curve_type[curve];
        curves[curve]->interpolation = (control_curve_interpolation_t)
            usb->curve_interpolation[curve];
        curves[curve]->point_count = usb->curve_point_count[curve];
        for (point = 0U; point < curves[curve]->point_count; point++) {
            curves[curve]->points[point] = (control_curve_point_t){
                .input = (float)usb->curve_point_millionths[curve][point][0] /
                         1000000.0F,
                .output =
                    (float)usb->curve_point_millionths[curve][point][1] /
                    1000000.0F,
            };
        }
    }
}

static const char *configuration_error(
    flight_configuration_service_result_t result)
{
    switch (result) {
    case FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE:
        return "state_rejected";
    case FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR:
        return "configuration_storage_error";
    case FLIGHT_CONFIGURATION_SERVICE_INVALID_ARGUMENT:
        return "configuration_invalid";
    case FLIGHT_CONFIGURATION_SERVICE_APPLY_ERROR:
        return "configuration_apply_error";
    case FLIGHT_CONFIGURATION_SERVICE_OK:
        break;
    }
    return "configuration_error";
}

static bool build_configuration_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    flight_configuration_t configuration;
    flight_configuration_source_t source;
    usb_json_configuration_t usb_configuration;
    bool accepted = true;
    const char *error = NULL;
    flight_configuration_service_result_t result =
        FLIGHT_CONFIGURATION_SERVICE_OK;

    saturating_increment(&processor->statistics.configuration_count);
    if (request->command == USB_JSON_COMMAND_CONFIG_WRITE) {
        configuration_from_usb(&request->configuration, &configuration);
        result = flight_configuration_service_write(
            processor->configuration_service, &configuration);
    } else if (request->command == USB_JSON_COMMAND_CONFIG_RESET) {
        result = flight_configuration_service_reset(
            processor->configuration_service);
    }
    accepted = result == FLIGHT_CONFIGURATION_SERVICE_OK;
    if (!accepted) {
        error = configuration_error(result);
    }
    if (!flight_configuration_service_read(
            processor->configuration_service, &configuration, &source)) {
        return false;
    }
    configuration_to_usb(&configuration, &usb_configuration);

    if (accepted) {
        saturating_increment(&processor->statistics.configuration_accepted_count);
    } else {
        saturating_increment(&processor->statistics.configuration_rejected_count);
    }
    return usb_json_build_configuration_response(
        request->command,
        request->request_id,
        accepted,
        flight_configuration_source_name(source),
        &usb_configuration,
        system_state_name(processor->state_machine->current),
        error,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static bool build_command_response(usb_command_processor_t *processor,
                                   const usb_json_request_t *request)
{
    switch (request->command) {
    case USB_JSON_COMMAND_STATUS:
        saturating_increment(&processor->statistics.status_count);
        return usb_json_build_status_response(
            system_state_name(processor->state_machine->current),
            motor_control_source_name(motor_control_active_source()),
            request->request_id,
            processor->clock(),
            processor->firmware_version,
            processor->build_id,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    case USB_JSON_COMMAND_HEALTH: {
        health_summary_t summary;

        saturating_increment(&processor->statistics.health_count);
        if (health_evaluate(processor->fault_system, &summary) !=
            HEALTH_EVALUATE_OK) {
            return false;
        }
        return usb_health_response_build(&summary,
                                         processor->fault_system,
                                         request->request_id,
                                         processor->pending_response,
                                         sizeof(processor->pending_response),
                                         &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_RECEIVER: {
        receiver_inspection_t inspection;

        saturating_increment(&processor->statistics.receiver_count);
        if (!processor->receiver_inspection_provider.read(
                processor->receiver_inspection_provider.context,
                &inspection)) {
            return build_error(processor,
                               true,
                               request->request_id,
                               "receiver_inspection_unavailable");
        }
        return usb_receiver_response_build(
            &inspection,
            request->request_id,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_IMU: {
        usb_imu_diagnostics_t diagnostics;

        saturating_increment(&processor->statistics.imu_count);
        if ((processor->state_machine->current == SYSTEM_STATE_ARMED) ||
            (processor->state_machine->current == SYSTEM_STATE_FAILSAFE)) {
            saturating_increment(&processor->statistics.imu_rejected_count);
            return build_error(processor,
                               true,
                               request->request_id,
                               "state_rejected");
        }
        build_imu_diagnostics(processor, &diagnostics);
        return usb_imu_response_build(
            &diagnostics,
            request->request_id,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_CONTROL_TRACE_START:
    case USB_JSON_COMMAND_CONTROL_TRACE_READ:
    case USB_JSON_COMMAND_CONTROL_TRACE_STOP:
        return build_control_trace_response(processor, request);
    case USB_JSON_COMMAND_ARM:
    case USB_JSON_COMMAND_DISARM: {
        const system_state_t previous = processor->state_machine->current;
        motor_control_arm_result_t arm_result =
            MOTOR_CONTROL_ARM_BLOCKED_STATE;
        motor_control_disarm_result_t disarm_result =
            MOTOR_CONTROL_DISARM_BLOCKED_STATE;
        const bool arm_requested = request->command == USB_JSON_COMMAND_ARM;
        bool arm_pending = false;
        const char *error = "transition_rejected";

        if (arm_requested) {
            arm_result = motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST);
            arm_pending = arm_result == MOTOR_CONTROL_ARM_PENDING;
            if (arm_result == MOTOR_CONTROL_ARM_BLOCKED_HEALTH) {
                error = "health_rejected";
            } else if (arm_result == MOTOR_CONTROL_ARM_BLOCKED_PREPARATION) {
                error = "motor_not_ready";
            } else if (arm_result == MOTOR_CONTROL_ARM_INVALID_SOURCE) {
                error = "control_source_rejected";
            }
            processor->last_transition_result =
                ((arm_result == MOTOR_CONTROL_ARM_ACCEPTED) || arm_pending)
                    ? SYSTEM_STATE_TRANSITION_OK
                    : SYSTEM_STATE_TRANSITION_REJECTED;
        } else {
            disarm_result = motor_control_disarm();
            processor->last_transition_result =
                disarm_result == MOTOR_CONTROL_DISARM_ACCEPTED
                    ? SYSTEM_STATE_TRANSITION_OK
                    : SYSTEM_STATE_TRANSITION_REJECTED;
        }
        processor->last_transition_valid = true;

        if (processor->last_transition_result == SYSTEM_STATE_TRANSITION_OK) {
            saturating_increment(
                &processor->statistics.transition_accepted_count);
            LOG_INFO(LOG_MODULE_STATE,
                     "%s -> %s source=usb",
                     system_state_name(previous),
                     system_state_name(processor->state_machine->current));
            return usb_json_build_transition_response(
                request->command,
                request->request_id,
                true,
                arm_pending,
                system_state_name(processor->state_machine->current),
                NULL,
                processor->pending_response,
                sizeof(processor->pending_response),
                &processor->pending_response_length);
        }

        saturating_increment(&processor->statistics.transition_rejected_count);
        return usb_json_build_transition_response(
            request->command,
            request->request_id,
            false,
            false,
            system_state_name(processor->state_machine->current),
            error,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_MOTOR_TEST:
        return build_motor_test_response(processor, request);
    case USB_JSON_COMMAND_CONFIG_READ:
    case USB_JSON_COMMAND_CONFIG_WRITE:
    case USB_JSON_COMMAND_CONFIG_RESET:
        return build_configuration_response(processor, request);
    case USB_JSON_COMMAND_UNSUPPORTED:
    case USB_JSON_COMMAND_INVALID:
        break;
    }

    return false;
}

usb_command_init_result_t usb_command_processor_initialize(
    usb_command_processor_t *processor,
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    usb_command_clock_t clock,
    const receiver_inspection_provider_t *receiver_inspection_provider,
    const imu_service_t *imu_service,
    const gyro_calibration_t *gyro_calibration,
    const imu_processing_pipeline_t *imu_processing_pipeline,
    const task_registry_t *task_registry,
    flight_configuration_service_t *configuration_service,
    control_trace_t *control_trace,
    const char *firmware_version,
    const char *build_id)
{
    if ((processor == NULL) || (state_machine == NULL) ||
        !state_machine->initialized || (fault_system == NULL) ||
        !fault_system->initialized || (clock == NULL) ||
        (receiver_inspection_provider == NULL) ||
        (receiver_inspection_provider->read == NULL) ||
        (imu_service == NULL) || (gyro_calibration == NULL) ||
        !gyro_calibration->initialized ||
        (imu_processing_pipeline == NULL) ||
        !imu_processing_pipeline->initialized ||
        (task_registry == NULL) ||
        (configuration_service == NULL) || !configuration_service->initialized ||
        (control_trace == NULL) || !control_trace->initialized ||
        (firmware_version == NULL) || (build_id == NULL)) {
        return USB_COMMAND_INIT_INVALID_ARGUMENT;
    }

    *processor = (usb_command_processor_t){
        .state_machine = state_machine,
        .fault_system = fault_system,
        .clock = clock,
        .receiver_inspection_provider = *receiver_inspection_provider,
        .imu_service = imu_service,
        .gyro_calibration = gyro_calibration,
        .imu_processing_pipeline = imu_processing_pipeline,
        .task_registry = task_registry,
        .configuration_service = configuration_service,
        .control_trace = control_trace,
        .firmware_version = firmware_version,
        .build_id = build_id,
        .initialized = true,
    };
    return USB_COMMAND_INIT_OK;
}

usb_command_process_result_t usb_command_processor_process_once(
    usb_command_processor_t *processor)
{
    uint8_t line[USB_CDC_RECEIVE_LINE_CAPACITY + 1U];
    size_t line_length;
    usb_cdc_line_result_t line_result;
    usb_json_request_t request;
    bool response_built;

    if ((processor == NULL) || !processor->initialized ||
        (processor->state_machine == NULL) ||
        !processor->state_machine->initialized ||
        (processor->fault_system == NULL) ||
        !processor->fault_system->initialized ||
        (processor->clock == NULL) ||
        (processor->receiver_inspection_provider.read == NULL) ||
        (processor->imu_service == NULL) ||
        (processor->task_registry == NULL) ||
        (processor->configuration_service == NULL) ||
        !processor->configuration_service->initialized ||
        (processor->control_trace == NULL) ||
        !processor->control_trace->initialized ||
        (processor->firmware_version == NULL) ||
        (processor->build_id == NULL)) {
        return USB_COMMAND_PROCESS_INVALID_STATE;
    }

    processor->last_transition_valid = false;

    if (processor->pending_response_valid) {
        return try_send_pending_response(processor);
    }

    line_result = usb_cdc_transport_read_line(line, sizeof(line), &line_length);
    if (line_result == USB_CDC_LINE_UNAVAILABLE) {
        return USB_COMMAND_PROCESS_IDLE;
    }
    if (line_result != USB_CDC_LINE_AVAILABLE) {
        return USB_COMMAND_PROCESS_INVALID_STATE;
    }

    saturating_increment(&processor->statistics.command_count);
    processor->pending_trace_discard_count = 0U;
    if (!usb_json_parse_request((const char *)line, line_length, &request)) {
        saturating_increment(&processor->statistics.malformed_count);
        response_built = build_error(processor,
                                     false,
                                     0U,
                                     "invalid_request");
    } else if (request.command == USB_JSON_COMMAND_UNSUPPORTED) {
        saturating_increment(&processor->statistics.unsupported_count);
        response_built = build_error(processor,
                                     true,
                                     request.request_id,
                                     "unsupported_command");
    } else {
        response_built = build_command_response(processor, &request);
    }

    if (!response_built) {
        processor->pending_response_length = 0U;
        saturating_increment(
            &processor->statistics.response_build_error_count);
        return USB_COMMAND_PROCESS_BUILD_ERROR;
    }

    processor->pending_response_valid = true;
    return try_send_pending_response(processor);
}
