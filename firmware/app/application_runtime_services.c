#include "application_runtime.h"

#include "application_state.h"
#include "application_tasks.h"
#include "bmi270_configuration.h"
#include "blackbox.h"
#include "fault_catalog.h"
#include "flight_configuration_service.h"
#include "flight_control.h"
#include "gyro_calibration.h"
#include "logging.h"
#include "manual_easy_behavior.h"
#include "motor_control.h"
#include "receiver_normalization.h"
#include "status_indicator.h"

#include <stdint.h>

#define RUNTIME_IMU_FRESH_THROUGH_US UINT32_C(2000)
#define RUNTIME_IMU_LOST_AFTER_US UINT32_C(10000)

static application_runtime_init_result_t fail_initialization(
    application_runtime_init_result_t result,
    boot_status_t boot_status,
    fault_id_t fault_id,
    uint32_t context)
{
    firmware_boot_status = boot_status;
    if (firmware_fault_system.initialized) {
        firmware_fault_last_result = (uint32_t)fault_system_report(
            &firmware_fault_system, fault_id, true, context);
    }
    if (firmware_system_state_machine.initialized &&
        (firmware_system_state_machine.current != SYSTEM_STATE_FAULT)) {
        firmware_system_state_last_result =
            (uint32_t)system_state_machine_handle_event(
                &firmware_system_state_machine,
                SYSTEM_STATE_EVENT_FAULT_DETECTED);
    }
    return result;
}

static bool services_are_valid(
    const application_runtime_services_t *services)
{
    return (services != NULL) && (services->clock != NULL) &&
           (services->configuration_storage.load != NULL) &&
           (services->configuration_storage.save != NULL) &&
           (services->configuration_storage.clear != NULL) &&
           (services->motor_output_backend.initialize != NULL) &&
           (services->motor_output_backend.submit != NULL) &&
           (services->motor_output_backend.submit_directions != NULL) &&
           (services->motor_output_backend.force_stop != NULL) &&
           (services->motor_output_backend.status != NULL) &&
           (services->motor_output_backend.diagnostic_context != NULL);
}

application_runtime_init_result_t
application_runtime_initialize_with_services(
    const application_runtime_services_t *services)
{
    const fault_definition_t *fault_definitions;
    size_t fault_definition_count;
    receiver_normalization_config_t normalization_config;
    receiver_freshness_config_t receiver_freshness;
    receiver_arming_config_t arming_config;
    const imu_axis_mapping_t axis_mapping = {
        .body_x = IMU_AXIS_POSITIVE_X,
        .body_y = IMU_AXIS_NEGATIVE_Y,
        .body_z = IMU_AXIS_NEGATIVE_Z,
    };
    const imu_freshness_config_t imu_freshness = {
        .fresh_through_us = RUNTIME_IMU_FRESH_THROUGH_US,
        .lost_after_us = RUNTIME_IMU_LOST_AFTER_US,
    };
    application_task_options_t task_options = {
        .receiver_service_available = false,
        .imu_service_available = false,
    };
    application_runtime_optional_tasks_t optional_tasks = {0};
    gyro_calibration_config_t gyro_config;
    system_state_transition_result_t transition;
    motor_control_init_result_t motor_result;

    if (!services_are_valid(services)) {
        return APPLICATION_RUNTIME_INIT_INVALID_ARGUMENT;
    }

    if (!firmware_system_state_machine.initialized &&
        !firmware_fault_system.initialized) {
        logging_initialize();
        system_state_machine_initialize(&firmware_system_state_machine);
        fault_definitions = firmware_fault_catalog(&fault_definition_count);
        if (fault_system_initialize(&firmware_fault_system,
                                    &firmware_system_state_machine,
                                    fault_definitions,
                                    fault_definition_count) != FAULT_INIT_OK) {
            return APPLICATION_RUNTIME_INIT_CORE_ERROR;
        }
        transition = system_state_machine_handle_event(
            &firmware_system_state_machine,
            SYSTEM_STATE_EVENT_INITIALIZATION_STARTED);
        firmware_system_state_last_result = (uint32_t)transition;
        if (transition != SYSTEM_STATE_TRANSITION_OK) {
            return fail_initialization(
                APPLICATION_RUNTIME_INIT_CORE_ERROR,
                BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR,
                FAULT_ID_STATE_MACHINE_TRANSITION,
                (uint32_t)transition);
        }
    } else if (!firmware_system_state_machine.initialized ||
               !firmware_fault_system.initialized ||
               ((firmware_system_state_machine.current !=
                 SYSTEM_STATE_INITIALIZING) &&
                (firmware_system_state_machine.current !=
                 SYSTEM_STATE_FAULT))) {
        return APPLICATION_RUNTIME_INIT_CORE_ERROR;
    }
    LOG_INFO(LOG_MODULE_SYSTEM, "OpenFlightComputer runtime initializing");
    if ((fault_system_attach_clock(&firmware_fault_system,
                                   services->clock) !=
         FAULT_CLOCK_ATTACH_OK) ||
        (logging_attach_clock(services->clock) !=
         LOGGING_CLOCK_ATTACH_OK)) {
        return fail_initialization(APPLICATION_RUNTIME_INIT_CLOCK_ERROR,
                                   BOOT_STATUS_FAULT_CLOCK_ATTACHMENT_ERROR,
                                   FAULT_ID_FAULT_CLOCK_ATTACHMENT,
                                   0U);
    }
    control_trace_initialize(&firmware_control_trace);

    if (flight_configuration_service_initialize(
            &firmware_flight_configuration_service,
            &services->configuration_storage,
            &firmware_system_state_machine,
            &firmware_receiver_failsafe,
            &firmware_receiver_service,
            &firmware_imu_processing_pipeline,
            &firmware_level_calibration,
            services->clock) != FLIGHT_CONFIGURATION_SERVICE_OK) {
        return fail_initialization(
            APPLICATION_RUNTIME_INIT_CONFIGURATION_ERROR,
            BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
            FAULT_ID_MOTOR_INITIALIZATION,
            0U);
    }

    motor_result = motor_control_initialize(
        &firmware_system_state_machine,
        &firmware_fault_system,
        services->clock,
        MOTOR_COMMAND_DEFAULT_TIMEOUT_US,
        &services->motor_output_backend,
        &firmware_flight_configuration_service.active.motors,
        status_indicator_motor_lifecycle_changed,
        NULL);
    firmware_motor_control_initialization_result = (uint32_t)motor_result;
    if ((motor_result != MOTOR_CONTROL_INIT_OK) ||
        !motor_control_set_external_arm_ready(
            firmware_flight_configuration_service.active
                .level_calibration.calibrated) ||
        !manual_easy_behavior_initialize(
            &firmware_manual_easy_behavior,
            &firmware_flight_configuration_service.active.easy_mode) ||
        !flight_control_core_initialize(
            &firmware_flight_control_core,
            &firmware_flight_configuration_service.rate_controller)) {
        return fail_initialization(APPLICATION_RUNTIME_INIT_MOTOR_ERROR,
                                   BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                                   FAULT_ID_MOTOR_INITIALIZATION,
                                   (uint32_t)motor_result);
    }

    if (services->imu_source.read != NULL) {
        if (!imu_service_initialize(&firmware_imu_service,
                                    &services->imu_source,
                                    services->clock,
                                    &axis_mapping,
                                    &imu_freshness)) {
            return fail_initialization(APPLICATION_RUNTIME_INIT_IMU_ERROR,
                                       BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                                       FAULT_ID_IMU_INITIALIZATION,
                                       0U);
        }
        firmware_imu_initialization_result = 0U;
        task_options.imu_service_available = true;
    }

    gyro_config = (gyro_calibration_config_t){
        .settling_duration_us =
            firmware_flight_configuration_service.active
                .gyro_calibration.settling_duration_us,
        .sample_duration_us =
            firmware_flight_configuration_service.active
                .gyro_calibration.sample_duration_us,
        .minimum_sample_count = 500U,
        .maximum_rate_dps =
            firmware_flight_configuration_service.active
                .gyro_calibration.maximum_rate_dps,
        .maximum_standard_deviation_dps =
            firmware_flight_configuration_service.active
                .gyro_calibration.maximum_standard_deviation_dps,
        .counts_per_dps = BMI270_GYROSCOPE_COUNTS_PER_DPS,
    };
    if (!gyro_calibration_initialize(&firmware_gyro_calibration,
                                     &gyro_config,
                                     services->clock())) {
        return fail_initialization(
            APPLICATION_RUNTIME_INIT_CALIBRATION_ERROR,
            BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
            FAULT_ID_IMU_INITIALIZATION,
            0U);
    }

    receiver_normalization_default_config(&normalization_config);
    receiver_arming_default_config(&arming_config);
    receiver_freshness = (receiver_freshness_config_t){
        .fresh_through_us =
            firmware_flight_configuration_service.active
                .receiver_failsafe.stale_after_us,
        .lost_after_us =
            firmware_flight_configuration_service.active
                .receiver_failsafe.loss_detected_after_us,
    };
    if (services->receiver_source.read != NULL) {
        if (!receiver_service_initialize(&firmware_receiver_service,
                                         &services->receiver_source,
                                         services->clock,
                                         &normalization_config,
                                         &receiver_freshness) ||
            !receiver_failsafe_initialize(
                &firmware_receiver_failsafe,
                &firmware_flight_configuration_service.active
                     .receiver_failsafe,
                services->clock()) ||
            !receiver_arming_initialize(&firmware_receiver_arming,
                                        &arming_config)) {
            return fail_initialization(
                APPLICATION_RUNTIME_INIT_RECEIVER_ERROR,
                BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                FAULT_ID_RECEIVER_INITIALIZATION,
                0U);
        }
        firmware_receiver_initialization_result = 0U;
        task_options.receiver_service_available = true;
    }

    blackbox_initialize(&firmware_blackbox,
                        NULL,
                        NULL,
                        0U,
                        "host",
                        "host");
    if (services->prepare_optional_services != NULL) {
        services->prepare_optional_services(
            services->optional_services_context, &optional_tasks);
    }
    task_options.usb_service = optional_tasks.usb_service;
    task_options.status_indicator = optional_tasks.status_indicator;
    task_options.blackbox_storage = optional_tasks.blackbox_storage;
    if (application_tasks_register(&task_options) != TASK_REGISTRATION_OK) {
        return fail_initialization(APPLICATION_RUNTIME_INIT_TASK_ERROR,
                                   BOOT_STATUS_TASK_REGISTRATION_ERROR,
                                   FAULT_ID_TASK_REGISTRATION,
                                   0U);
    }
    if (scheduler_initialize(&firmware_scheduler,
                             &firmware_task_registry,
                             services->clock) != SCHEDULER_INIT_OK) {
        return fail_initialization(APPLICATION_RUNTIME_INIT_SCHEDULER_ERROR,
                                   BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR,
                                   FAULT_ID_SCHEDULER_INITIALIZATION,
                                   0U);
    }
    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZED;
    return APPLICATION_RUNTIME_INIT_OK;
}

application_runtime_drain_result_t application_runtime_run_until_idle(
    size_t maximum_task_executions,
    size_t *task_executions)
{
    size_t executed = 0U;

    if (task_executions != NULL) {
        *task_executions = 0U;
    }
    if ((maximum_task_executions == 0U) ||
        !firmware_scheduler.initialized ||
        (firmware_scheduler.clock == NULL)) {
        return APPLICATION_RUNTIME_DRAIN_INVALID_ARGUMENT;
    }
    while (executed < maximum_task_executions) {
        const scheduler_step_result_t result =
            scheduler_run_once(&firmware_scheduler);

        firmware_scheduler_last_result = (uint32_t)result;
        if (result == SCHEDULER_STEP_IDLE) {
            if (task_executions != NULL) {
                *task_executions = executed;
            }
            firmware_uptime_us = firmware_scheduler.clock();
            firmware_main_loop_iterations++;
            return APPLICATION_RUNTIME_DRAIN_IDLE;
        }
        if (result != SCHEDULER_STEP_TASK_EXECUTED) {
            if (task_executions != NULL) {
                *task_executions = executed;
            }
            return APPLICATION_RUNTIME_DRAIN_SCHEDULER_ERROR;
        }
        executed++;
        firmware_main_loop_iterations++;
    }
    if (task_executions != NULL) {
        *task_executions = executed;
    }
    firmware_uptime_us = firmware_scheduler.clock();
    return APPLICATION_RUNTIME_DRAIN_LIMIT_REACHED;
}
