#include "application_runtime.h"

#include "application_state.h"
#include "application_task_definitions.h"
#include "application_tasks.h"
#include "bmi270_configuration.h"
#include "bmi270_driver.h"
#include "board.h"
#include "board_usb.h"
#include "board_flight_configuration_storage.h"
#include "board_imu.h"
#include "board_receiver.h"
#include "board_sd_card.h"
#include "dshot_motor_backend.h"
#include "fault_catalog.h"
#include "firmware_identity.h"
#include "imu_service.h"
#include "gyro_calibration.h"
#include "level_calibration.h"
#include "logging.h"
#include "motor_control.h"
#include "motor_control_internal.h"
#include "receiver_normalization.h"
#include "time.h"
#include "usb_cdc_transport.h"
#include "usb_logging_backend.h"
#include "status_indicator.h"
#include "sd_card.h"

#include <stddef.h>

#define USB_LOGGING_FAULT_CONTEXT_BACKEND_ATTACHMENT UINT32_C(100)
static dshot_motor_backend_t firmware_dshot_motor_backend;
static bmi270_driver_t firmware_bmi270_driver;
static sd_card_t firmware_sd_card;
static uint8_t
    firmware_blackbox_configuration[FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY];

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

static boot_status_t boot_status_for_runtime_error(
    application_runtime_init_result_t result)
{
    switch (result) {
    case APPLICATION_RUNTIME_INIT_CLOCK_ERROR:
        return BOOT_STATUS_FAULT_CLOCK_ATTACHMENT_ERROR;
    case APPLICATION_RUNTIME_INIT_TASK_ERROR:
        return BOOT_STATUS_TASK_REGISTRATION_ERROR;
    case APPLICATION_RUNTIME_INIT_SCHEDULER_ERROR:
        return BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR;
    case APPLICATION_RUNTIME_INIT_CORE_ERROR:
        return BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR;
    case APPLICATION_RUNTIME_INIT_INVALID_ARGUMENT:
    case APPLICATION_RUNTIME_INIT_CONFIGURATION_ERROR:
    case APPLICATION_RUNTIME_INIT_MOTOR_ERROR:
    case APPLICATION_RUNTIME_INIT_IMU_ERROR:
    case APPLICATION_RUNTIME_INIT_RECEIVER_ERROR:
    case APPLICATION_RUNTIME_INIT_CALIBRATION_ERROR:
        return BOOT_STATUS_MOTOR_INITIALIZATION_ERROR;
    case APPLICATION_RUNTIME_INIT_OK:
        break;
    }
    return BOOT_STATUS_MOTOR_INITIALIZATION_ERROR;
}

static void halt_after_runtime_error(application_runtime_init_result_t result)
{
    firmware_boot_status = boot_status_for_runtime_error(result);
    LOG_FATAL(LOG_MODULE_FAULT,
              "runtime initialization failed result=%u boot_status=%u",
              (unsigned int)result,
              (unsigned int)firmware_boot_status);
    board_halt();
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
    status_indicator_attach_backend(board_status_indicator_set_rgb);
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

static void prepare_motor_services(
    motor_output_backend_t *motor_output_backend,
    flight_configuration_storage_t *configuration_storage)
{
    if (!dshot_motor_backend_prepare(&firmware_dshot_motor_backend,
                                     motor_output_backend)) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        false,
                        0U);
    }
    *configuration_storage = board_flight_configuration_storage();
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

static bool initialize_imu_source(imu_source_t *source)
{
    bmi270_raw_sample_t sample;
    bmi270_driver_init_result_t initialize_result;

    if (source == NULL) {
        return false;
    }
    *source = (imu_source_t){0};

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
    *source = (imu_source_t){
        .read = read_bmi270,
        .context = &firmware_bmi270_driver,
    };
    return true;
}

static bool initialize_receiver_source(receiver_source_t *receiver_source)
{
    const board_receiver_init_result_t result =
        board_receiver_initialize(receiver_source);

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

    LOG_INFO(LOG_MODULE_RECEIVER, "UART4 CRSF receiver initialized");
    return true;
}

static bool initialize_usb(void)
{
    (void)board_usb_select_automatic_vbus_mode();
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
            &firmware_level_calibration,
            &firmware_imu_processing_pipeline,
            &firmware_task_registry,
            &firmware_flight_configuration_service,
            &firmware_control_trace,
            &firmware_blackbox,
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

static void initialize_blackbox(void)
{
    size_t configuration_length = 0U;
    const sd_card_result_t card_result = sd_card_initialize(
        &firmware_sd_card,
        board_sd_card_spi_device(),
        time_us,
        board_sd_card_inserted());

    if (!board_flight_configuration_snapshot_encode(
            &firmware_flight_configuration_service.active,
            firmware_blackbox_configuration,
            sizeof(firmware_blackbox_configuration),
            &configuration_length)) {
        configuration_length = 0U;
    }
    blackbox_initialize(&firmware_blackbox,
                        card_result == SD_CARD_RESULT_OK
                            ? &firmware_sd_card
                            : NULL,
                        firmware_blackbox_configuration,
                        configuration_length,
                        firmware_version,
                        firmware_build_id);
    if (card_result == SD_CARD_RESULT_NO_MEDIA) {
        LOG_WARN(LOG_MODULE_SYSTEM, "blackbox SD card not present");
    } else if (card_result != SD_CARD_RESULT_OK) {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "blackbox SD initialization failed result=%u",
                  (unsigned int)card_result);
    } else {
        LOG_INFO(LOG_MODULE_SYSTEM,
                 "blackbox SD ready sectors=%lu status=%s",
                 (unsigned long)firmware_sd_card.sector_count,
                 blackbox_status_name(firmware_blackbox.status));
    }
}

static void prepare_optional_board_services(
    void *context,
    application_runtime_optional_tasks_t *tasks)
{
    (void)context;
    if (tasks == NULL) {
        return;
    }
    initialize_blackbox();
    *tasks = (application_runtime_optional_tasks_t){
        .usb_service = initialize_usb()
                           ? usb_service_task_definition()
                           : NULL,
        .status_indicator = status_indicator_task_definition(),
        .blackbox_storage = blackbox_storage_task_definition(),
    };
}

void application_runtime_initialize(void)
{
    application_runtime_services_t services = {
        .clock = time_us,
        .prepare_optional_services = prepare_optional_board_services,
    };
    application_runtime_init_result_t result;

    initialize_core();
    initialize_board();
    (void)initialize_imu_source(&services.imu_source);
    (void)initialize_receiver_source(&services.receiver_source);
    prepare_motor_services(&services.motor_output_backend,
                           &services.configuration_storage);
    result = application_runtime_initialize_with_services(&services);
    if (result != APPLICATION_RUNTIME_INIT_OK) {
        halt_after_runtime_error(result);
    }

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
        firmware_uptime_us = firmware_scheduler.clock();
        firmware_main_loop_iterations++;
    }
}
