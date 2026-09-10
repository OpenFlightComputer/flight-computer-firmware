#include "application_runtime.h"

#include "application_state.h"
#include "application_tasks.h"
#include "bmi270_driver.h"
#include "board.h"
#include "board_flight_configuration_storage.h"
#include "board_imu.h"
#include "board_receiver.h"
#include "dshot_motor_backend.h"
#include "fault_catalog.h"
#include "firmware_identity.h"
#include "imu_service.h"
#include "gyro_calibration.h"
#include "logging.h"
#include "motor_control.h"
#include "motor_control_internal.h"
#include "receiver_normalization.h"
#include "time.h"
#include "usb_cdc_transport.h"
#include "usb_logging_backend.h"
#include "status_indicator.h"

#include <stddef.h>

#define USB_LOGGING_FAULT_CONTEXT_BACKEND_ATTACHMENT UINT32_C(100)
#define IMU_FRESH_THROUGH_US UINT32_C(2000)
#define IMU_LOST_AFTER_US UINT32_C(10000)

static dshot_motor_backend_t firmware_dshot_motor_backend;
static bmi270_driver_t firmware_bmi270_driver;

static void stop_with_fault(boot_status_t status,
                            fault_id_t fault_id,
                            bool context_valid,
                            uint32_t context)
{
    LOG_FATAL(LOG_MODULE_FAULT,
              "id=%u boot_status=%u context_valid=%u context=%lu",
              (unsigned int)fault_id,
              (unsigned int)status,
              context_valid ? 1U : 0U,
              (unsigned long)context);
    firmware_fault_last_result =
        (uint32_t)fault_system_report(&firmware_fault_system,
                                      fault_id,
                                      context_valid,
                                      context);
    if (firmware_system_state_machine.current == SYSTEM_STATE_FAULT) {
        firmware_system_state_last_result =
            (uint32_t)SYSTEM_STATE_TRANSITION_OK;
    } else {
        firmware_system_state_last_result =
            (uint32_t)system_state_machine_handle_event(
                &firmware_system_state_machine,
                SYSTEM_STATE_EVENT_FAULT_DETECTED);
    }
    firmware_boot_status = status;
    board_halt();
}

static bool transition_system_state(system_state_event_t event)
{
    const system_state_transition_result_t result =
        system_state_machine_handle_event(&firmware_system_state_machine,
                                          event);

    firmware_system_state_last_result = (uint32_t)result;
    return result == SYSTEM_STATE_TRANSITION_OK;
}

static boot_status_t boot_status_for_board_error(board_init_result_t result)
{
    switch (result) {
    case BOARD_INIT_MCU_ERROR:
        return BOOT_STATUS_MCU_INITIALIZATION_ERROR;
    case BOARD_INIT_CLOCK_CONFIGURATION_ERROR:
        return BOOT_STATUS_CLOCK_CONFIGURATION_ERROR;
    case BOARD_INIT_CLOCK_FREQUENCY_ERROR:
        return BOOT_STATUS_CLOCK_FREQUENCY_ERROR;
    case BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR:
        return BOOT_STATUS_TIMEBASE_CONFIGURATION_ERROR;
    case BOARD_INIT_STATUS_INDICATOR_ERROR:
        return BOOT_STATUS_MCU_INITIALIZATION_ERROR;
    case BOARD_INIT_OK:
        break;
    }
    return BOOT_STATUS_MCU_INITIALIZATION_ERROR;
}

static fault_id_t fault_id_for_board_error(board_init_result_t result)
{
    switch (result) {
    case BOARD_INIT_MCU_ERROR:
        return FAULT_ID_MCU_INITIALIZATION;
    case BOARD_INIT_CLOCK_CONFIGURATION_ERROR:
        return FAULT_ID_CLOCK_CONFIGURATION;
    case BOARD_INIT_CLOCK_FREQUENCY_ERROR:
        return FAULT_ID_CLOCK_FREQUENCY;
    case BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR:
        return FAULT_ID_TIMEBASE_CONFIGURATION;
    case BOARD_INIT_STATUS_INDICATOR_ERROR:
        return FAULT_ID_MCU_INITIALIZATION;
    case BOARD_INIT_OK:
        break;
    }
    return FAULT_ID_MCU_INITIALIZATION;
}

static void initialize_core(void)
{
    const fault_definition_t *fault_definitions;
    size_t fault_definition_count;

    logging_initialize();
    LOG_INFO(LOG_MODULE_SYSTEM,
             "OpenFlightComputer %s booting",
             firmware_version_string);
    system_state_machine_initialize(&firmware_system_state_machine);
    fault_definitions = firmware_fault_catalog(&fault_definition_count);
    if (fault_system_initialize(&firmware_fault_system,
                                &firmware_system_state_machine,
                                fault_definitions,
                                fault_definition_count) != FAULT_INIT_OK) {
        stop_with_fault(BOOT_STATUS_FAULT_SYSTEM_INITIALIZATION_ERROR,
                        FAULT_ID_INVALID,
                        false,
                        0U);
    }
    if (!transition_system_state(SYSTEM_STATE_EVENT_INITIALIZATION_STARTED)) {
        stop_with_fault(BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR,
                        FAULT_ID_STATE_MACHINE_TRANSITION,
                        true,
                        (uint32_t)SYSTEM_STATE_EVENT_INITIALIZATION_STARTED);
    }
}

static void initialize_board(void)
{
    board_init_result_t result;

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZATION_STARTED;
    result = board_initialize();
    if (result != BOARD_INIT_OK) {
        stop_with_fault(boot_status_for_board_error(result),
                        fault_id_for_board_error(result),
                        true,
                        (uint32_t)result);
    }
    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZED;
    if (fault_system_attach_clock(&firmware_fault_system, time_us) !=
        FAULT_CLOCK_ATTACH_OK) {
        stop_with_fault(BOOT_STATUS_FAULT_CLOCK_ATTACHMENT_ERROR,
                        FAULT_ID_FAULT_CLOCK_ATTACHMENT,
                        false,
                        0U);
    }
    if (logging_attach_clock(time_us) != LOGGING_CLOCK_ATTACH_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_LOGGING_CLOCK_ATTACHMENT,
                                          false,
                                          0U);
        LOG_ERROR(LOG_MODULE_SYSTEM, "logging clock attachment failed");
    }
    LOG_INFO(LOG_MODULE_BOARD, "Flight Computer V1 initialized");
    (void)status_indicator_show_state(SYSTEM_STATE_INITIALIZING);
}

static void initialize_motor_control(void)
{
    motor_output_backend_t motor_output_backend;
    flight_configuration_storage_t configuration_storage;
    motor_control_init_result_t result;

    if (!dshot_motor_backend_prepare(&firmware_dshot_motor_backend,
                                     &motor_output_backend)) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        false,
                        0U);
    }
    configuration_storage = board_flight_configuration_storage();
    if (flight_configuration_service_initialize(
            &firmware_flight_configuration_service,
            &configuration_storage,
            &firmware_system_state_machine,
            &firmware_receiver_failsafe,
            &firmware_receiver_service,
            time_us) != FLIGHT_CONFIGURATION_SERVICE_OK) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        false,
                        0U);
    }
    result = motor_control_initialize(
        &firmware_system_state_machine,
        &firmware_fault_system,
        time_us,
        MOTOR_COMMAND_DEFAULT_TIMEOUT_US,
        &motor_output_backend,
        &firmware_flight_configuration_service.active.motors,
        status_indicator_motor_lifecycle_changed,
        NULL);
    firmware_motor_control_initialization_result = (uint32_t)result;
    if (result != MOTOR_CONTROL_INIT_OK) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        true,
                        (uint32_t)result);
    }
    LOG_INFO(LOG_MODULE_SYSTEM, "four-channel DShot300 output initialized");
}

static imu_source_result_t read_bmi270(void *context,
                                      imu_raw_sample_t *sample)
{
    bmi270_raw_sample_t bmi270_sample;
    bmi270_driver_t *driver = context;

    if ((driver == NULL) || (sample == NULL) ||
        (bmi270_driver_read_raw(driver, &bmi270_sample) !=
         BMI270_DRIVER_SAMPLE_OK)) {
        return IMU_SOURCE_ERROR;
    }
    *sample = (imu_raw_sample_t){
        .acceleration_x = bmi270_sample.acceleration_x,
        .acceleration_y = bmi270_sample.acceleration_y,
        .acceleration_z = bmi270_sample.acceleration_z,
        .gyroscope_x = bmi270_sample.gyroscope_x,
        .gyroscope_y = bmi270_sample.gyroscope_y,
        .gyroscope_z = bmi270_sample.gyroscope_z,
    };
    return IMU_SOURCE_SAMPLE_AVAILABLE;
}

static bool initialize_imu(void)
{
    bmi270_raw_sample_t sample;
    bmi270_driver_init_result_t initialize_result;
    const imu_source_t source = {
        .read = read_bmi270,
        .context = &firmware_bmi270_driver,
    };
    /*
     * V1 installation convention: PCB top is aircraft forward and the
     * component side faces up. The package is unrotated in the authoritative
     * PCB, giving body forward=+sensor Y, right=+sensor X, down=-sensor Z.
     * Milestone 4.3 must verify all signs physically before control use.
     */
    const imu_axis_mapping_t axis_mapping = {
        .body_x = IMU_AXIS_POSITIVE_Y,
        .body_y = IMU_AXIS_POSITIVE_X,
        .body_z = IMU_AXIS_NEGATIVE_Z,
    };
    const imu_freshness_config_t freshness_config = {
        .fresh_through_us = IMU_FRESH_THROUGH_US,
        .lost_after_us = IMU_LOST_AFTER_US,
    };

    firmware_imu_initial_sample_result =
        (uint32_t)BMI270_DRIVER_SAMPLE_NOT_INITIALIZED;
    initialize_result = bmi270_driver_initialize(&firmware_bmi270_driver,
                                                 board_imu_spi_device());

    firmware_imu_initialization_result = (uint32_t)initialize_result;
    if (initialize_result != BMI270_DRIVER_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(
                &firmware_fault_system,
                FAULT_ID_IMU_INITIALIZATION,
                true,
                ((uint32_t)initialize_result << 8U) |
                    (uint8_t)firmware_bmi270_driver.last_sensor_result);
        LOG_ERROR(LOG_MODULE_IMU,
                  "BMI270 initialization failed result=%u sensor=%d",
                  (unsigned int)initialize_result,
                  (int)firmware_bmi270_driver.last_sensor_result);
        return false;
    }

    firmware_imu_initial_sample_result =
        (uint32_t)bmi270_driver_read_raw(&firmware_bmi270_driver, &sample);
    if (firmware_imu_initial_sample_result !=
        (uint32_t)BMI270_DRIVER_SAMPLE_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(
                &firmware_fault_system,
                FAULT_ID_IMU_INITIALIZATION,
                true,
                (UINT32_C(1) << 16U) |
                    (firmware_imu_initial_sample_result << 8U) |
                    (uint8_t)firmware_bmi270_driver.last_sensor_result);
        LOG_ERROR(LOG_MODULE_IMU,
                  "BMI270 initial sample failed result=%lu sensor=%d",
                  (unsigned long)firmware_imu_initial_sample_result,
                  (int)firmware_bmi270_driver.last_sensor_result);
        return false;
    }

    firmware_imu_raw_acceleration_x = sample.acceleration_x;
    firmware_imu_raw_acceleration_y = sample.acceleration_y;
    firmware_imu_raw_acceleration_z = sample.acceleration_z;
    firmware_imu_raw_gyroscope_x = sample.gyroscope_x;
    firmware_imu_raw_gyroscope_y = sample.gyroscope_y;
    firmware_imu_raw_gyroscope_z = sample.gyroscope_z;
    LOG_INFO(LOG_MODULE_IMU,
             "BMI270 initialized raw accel=%d,%d,%d gyro=%d,%d,%d",
             (int)sample.acceleration_x,
             (int)sample.acceleration_y,
             (int)sample.acceleration_z,
             (int)sample.gyroscope_x,
             (int)sample.gyroscope_y,
             (int)sample.gyroscope_z);
    if (!imu_service_initialize(&firmware_imu_service,
                                &source,
                                time_us,
                                &axis_mapping,
                                &freshness_config)) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_IMU_INITIALIZATION,
                                          true,
                                          UINT32_C(0x020000));
        LOG_ERROR(LOG_MODULE_IMU, "IMU service initialization failed");
        return false;
    }
    return true;
}

static bool initialize_receiver(void)
{
    receiver_source_t receiver_source;
    receiver_normalization_config_t normalization_config;
    receiver_freshness_config_t freshness_config;
    receiver_arming_config_t arming_config;
    const board_receiver_init_result_t result =
        board_receiver_initialize(&receiver_source);

    firmware_receiver_initialization_result = (uint32_t)result;
    if (result != BOARD_RECEIVER_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_RECEIVER_INITIALIZATION,
                                          true,
                                          (uint32_t)result);
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "receiver initialization failed result=%u",
                  (unsigned int)result);
        return false;
    }

    receiver_normalization_default_config(&normalization_config);
    receiver_arming_default_config(&arming_config);
    freshness_config = (receiver_freshness_config_t){
        .fresh_through_us = firmware_flight_configuration_service.active
                                .receiver_failsafe.stale_after_us,
        .lost_after_us = firmware_flight_configuration_service.active
                             .receiver_failsafe.loss_detected_after_us,
    };
    if (receiver_service_initialize(&firmware_receiver_service,
                                    &receiver_source,
                                    time_us,
                                    &normalization_config,
                                    &freshness_config) &&
        receiver_failsafe_initialize(&firmware_receiver_failsafe,
                                     &firmware_flight_configuration_service
                                          .active.receiver_failsafe,
                                     time_us()) &&
        receiver_arming_initialize(&firmware_receiver_arming,
                                   &arming_config)) {
        LOG_INFO(LOG_MODULE_RECEIVER, "UART4 CRSF receiver initialized");
        return true;
    }

    firmware_fault_last_result =
        (uint32_t)fault_system_report(
            &firmware_fault_system,
            FAULT_ID_RECEIVER_INITIALIZATION,
            true,
            (uint32_t)BOARD_RECEIVER_INIT_SOURCE_ERROR);
    LOG_ERROR(LOG_MODULE_RECEIVER, "receiver service initialization failed");
    return false;
}

static bool initialize_usb(void)
{
    const usb_cdc_init_result_t result = usb_cdc_transport_initialize();

    firmware_usb_initialization_result = (uint32_t)result;
    if (result != USB_CDC_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_USB_LOGGING_INITIALIZATION,
                                          true,
                                          (uint32_t)result);
        LOG_ERROR(LOG_MODULE_USB,
                  "CDC initialization failed result=%u",
                  (unsigned int)result);
        return false;
    }
    {
        const logging_backend_t backend = usb_logging_backend();

        if (logging_attach_backend(&backend) != LOGGING_BACKEND_ATTACH_OK) {
            firmware_fault_last_result =
                (uint32_t)fault_system_report(
                    &firmware_fault_system,
                    FAULT_ID_USB_LOGGING_INITIALIZATION,
                    true,
                    USB_LOGGING_FAULT_CONTEXT_BACKEND_ATTACHMENT);
            LOG_ERROR(LOG_MODULE_USB, "logging backend attachment failed");
            return false;
        }
    }
    if (usb_command_processor_initialize(
            &firmware_usb_command_processor,
            &firmware_system_state_machine,
            &firmware_fault_system,
            time_us,
            application_receiver_inspection_provider(),
            &firmware_imu_service,
            &firmware_gyro_calibration,
            &firmware_task_registry,
            &firmware_flight_configuration_service,
            firmware_version,
            firmware_build_id) != USB_COMMAND_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_USB_COMMAND_INITIALIZATION,
                                          false,
                                          0U);
        LOG_ERROR(LOG_MODULE_USB, "command processor initialization failed");
        return false;
    }
    LOG_INFO(LOG_MODULE_USB, "CDC JSON service initialized");
    return true;
}

static void initialize_scheduler(bool usb_available,
                                 bool receiver_available,
                                 bool imu_available)
{
    const task_registration_result_t task_result =
        application_tasks_register(usb_available,
                                   receiver_available,
                                   imu_available);
    scheduler_init_result_t scheduler_result;

    if (task_result != TASK_REGISTRATION_OK) {
        stop_with_fault(BOOT_STATUS_TASK_REGISTRATION_ERROR,
                        FAULT_ID_TASK_REGISTRATION,
                        true,
                        (uint32_t)task_result);
    }
    LOG_INFO(LOG_MODULE_TASK,
             "task registry initialized count=%u",
             (unsigned int)task_registry_count(&firmware_task_registry));
    scheduler_result = scheduler_initialize(&firmware_scheduler,
                                            &firmware_task_registry,
                                            time_us);
    if (scheduler_result != SCHEDULER_INIT_OK) {
        stop_with_fault(BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR,
                        FAULT_ID_SCHEDULER_INITIALIZATION,
                        true,
                        (uint32_t)scheduler_result);
    }
    LOG_INFO(LOG_MODULE_SCHEDULER, "scheduler initialized");
}

void application_runtime_initialize(void)
{
    bool receiver_available;
    bool usb_available;
    bool imu_available;

    initialize_core();
    initialize_board();
    imu_available = initialize_imu();
    initialize_motor_control();
    {
        const gyro_calibration_configuration_t *configuration =
            &firmware_flight_configuration_service.active.gyro_calibration;
        const gyro_calibration_config_t calibration_config = {
            .settling_duration_us = configuration->settling_duration_us,
            .sample_duration_us = configuration->sample_duration_us,
            .minimum_sample_count = 500U,
            .maximum_rate_dps = configuration->maximum_rate_dps,
            .maximum_standard_deviation_dps =
                configuration->maximum_standard_deviation_dps,
            .counts_per_dps = 16.384F,
        };

        if (!gyro_calibration_initialize(&firmware_gyro_calibration,
                                         &calibration_config,
                                         time_us())) {
            stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                            FAULT_ID_IMU_INITIALIZATION,
                            false,
                            0U);
        }
    }
    receiver_available = initialize_receiver();
    usb_available = initialize_usb();
    initialize_scheduler(usb_available, receiver_available, imu_available);

    LOG_INFO(LOG_MODULE_SYSTEM, "waiting for startup calibration");
}

void application_runtime_run(void)
{
    for (;;) {
        const scheduler_step_result_t result =
            scheduler_run_once(&firmware_scheduler);

        firmware_scheduler_last_result = (uint32_t)result;
        if (result == SCHEDULER_STEP_INVALID_STATE) {
            stop_with_fault(BOOT_STATUS_SCHEDULER_RUNTIME_ERROR,
                            FAULT_ID_SCHEDULER_RUNTIME,
                            true,
                            (uint32_t)result);
        }
        firmware_uptime_us = time_us();
        firmware_main_loop_iterations++;
    }
}
