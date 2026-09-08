#include "boot_status.h"
#include "board.h"
#include "board_receiver.h"
#include "dshot_motor_backend.h"
#include "fault.h"
#include "fault_catalog.h"
#include "firmware_identity.h"
#include "logging.h"
#include "motor_control.h"
#include "motor_control_internal.h"
#include "receiver_failsafe.h"
#include "receiver_service.h"
#include "scheduler.h"
#include "system_state.h"
#include "task.h"
#include "time.h"
#include "usb_cdc_transport.h"
#include "usb_command_processor.h"
#include "usb_logging_backend.h"

#include <stddef.h>

#define USB_LOGGING_FAULT_CONTEXT_BACKEND_ATTACHMENT UINT32_C(100)
#define RECEIVER_TASK_PERIOD_US UINT32_C(1000)

volatile boot_status_t firmware_boot_status = BOOT_STATUS_RESET;
volatile uint32_t firmware_main_loop_iterations;
volatile uint64_t firmware_uptime_us;
volatile uint32_t firmware_scheduler_last_result;
volatile uint32_t firmware_fast_task_executions;
volatile uint32_t firmware_medium_task_executions;
volatile uint32_t firmware_slow_task_executions;
volatile uint32_t firmware_system_state_last_result;
volatile uint32_t firmware_fault_last_result = UINT32_MAX;
volatile uint32_t firmware_usb_initialization_result = UINT32_MAX;
volatile uint32_t firmware_logging_drain_last_result = UINT32_MAX;
volatile uint32_t firmware_usb_command_last_result = UINT32_MAX;
volatile uint32_t firmware_usb_service_task_executions;
volatile uint32_t firmware_motor_control_initialization_result = UINT32_MAX;
volatile uint32_t firmware_motor_control_sync_last_result = UINT32_MAX;
volatile uint32_t firmware_motor_control_task_executions;
volatile uint32_t firmware_receiver_initialization_result = UINT32_MAX;
volatile uint32_t firmware_receiver_service_last_result = UINT32_MAX;
volatile uint32_t firmware_receiver_task_executions;
volatile uint32_t firmware_receiver_freshness =
    (uint32_t)RECEIVER_FRESHNESS_UNAVAILABLE;
volatile uint32_t firmware_receiver_uart_bytes;
volatile uint32_t firmware_receiver_valid_frames;
volatile uint32_t firmware_receiver_crc_errors;
volatile uint32_t firmware_receiver_framing_errors;
volatile uint32_t firmware_receiver_dma_overruns;
volatile uint32_t firmware_receiver_dma_dropped_bytes;
volatile uint32_t firmware_receiver_failsafe_state =
    (uint32_t)RECEIVER_FAILSAFE_UNAVAILABLE;
volatile uint32_t firmware_receiver_failsafe_action =
    (uint32_t)RECEIVER_FAILSAFE_ACTION_NONE;
volatile uint64_t firmware_receiver_link_age_us;
volatile uint32_t firmware_receiver_failsafe_transitions;
volatile bool firmware_receiver_stage_two_latched;
volatile bool firmware_receiver_recovery_ready;

task_registry_t firmware_task_registry;
scheduler_t firmware_scheduler;
system_state_machine_t firmware_system_state_machine;
fault_system_t firmware_fault_system;
usb_command_processor_t firmware_usb_command_processor;
static dshot_motor_backend_t firmware_dshot_motor_backend;
receiver_service_t firmware_receiver_service;
receiver_failsafe_t firmware_receiver_failsafe;
receiver_failsafe_decision_t firmware_receiver_failsafe_decision;
static receiver_freshness_state_t logged_receiver_freshness =
    RECEIVER_FRESHNESS_UNAVAILABLE;
static receiver_failsafe_state_t logged_receiver_failsafe_state =
    RECEIVER_FAILSAFE_UNAVAILABLE;
static bool receiver_source_fault_reported;
static bool receiver_connection_fault_reported;

static void motor_control_task(void *context)
{
    (void)context;

    firmware_motor_control_sync_last_result =
        (uint32_t)motor_control_synchronize();
    firmware_motor_control_task_executions++;
}

static void receiver_task(void *context)
{
    receiver_service_t *service = context;
    receiver_control_state_t control;
    board_receiver_statistics_t statistics = {0};
    receiver_service_result_t result;
    receiver_failsafe_decision_t decision;
    const receiver_control_snapshot_t *control_snapshot = NULL;

    result = receiver_service_process_once(service);
    firmware_receiver_service_last_result = (uint32_t)result;
    firmware_receiver_task_executions++;

    if (board_receiver_statistics(&statistics)) {
        firmware_receiver_uart_bytes = statistics.uart_received_byte_count;
        firmware_receiver_valid_frames = statistics.valid_frame_count;
        firmware_receiver_crc_errors = statistics.crc_error_count;
        firmware_receiver_framing_errors = statistics.framing_error_count;
        firmware_receiver_dma_overruns = statistics.dma_overrun_count;
        firmware_receiver_dma_dropped_bytes =
            statistics.dma_dropped_byte_count;
    }

    if ((result == RECEIVER_SERVICE_SOURCE_ERROR) &&
        !receiver_source_fault_reported) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_RECEIVER_SOURCE,
                                          true,
                                          statistics.uart_error);
        receiver_source_fault_reported = true;
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "source error context=%lu",
                  (unsigned long)statistics.uart_error);
    }

    if (receiver_service_control_state(service, &control)) {
        firmware_receiver_freshness = (uint32_t)control.freshness;
        if (control.freshness != logged_receiver_freshness) {
            if (control.freshness == RECEIVER_FRESHNESS_FRESH) {
                LOG_INFO(LOG_MODULE_RECEIVER, "receiver data fresh");
            } else if (control.freshness == RECEIVER_FRESHNESS_STALE) {
                LOG_WARN(LOG_MODULE_RECEIVER, "receiver data stale");
            } else if (control.freshness == RECEIVER_FRESHNESS_LOST) {
                LOG_ERROR(LOG_MODULE_RECEIVER, "receiver data lost");
            }
            logged_receiver_freshness = control.freshness;
        }

        if (control.snapshot.valid) {
            control_snapshot = &control.snapshot;
        }
    }

    if (!receiver_failsafe_update(&firmware_receiver_failsafe,
                                  control_snapshot,
                                  time_us(),
                                  &decision)) {
        return;
    }

    firmware_receiver_failsafe_decision = decision;
    firmware_receiver_failsafe_state = (uint32_t)decision.state;
    firmware_receiver_failsafe_action = (uint32_t)decision.action;
    firmware_receiver_link_age_us = decision.link_age_us;
    firmware_receiver_failsafe_transitions = decision.transition_count;
    firmware_receiver_stage_two_latched = decision.stage_two_latched;
    firmware_receiver_recovery_ready = decision.recovery_ready;

    if (decision.state != logged_receiver_failsafe_state) {
        if ((decision.state == RECEIVER_FAILSAFE_LIVE) ||
            (decision.state == RECEIVER_FAILSAFE_STALE_HOLD)) {
            LOG_INFO(LOG_MODULE_RECEIVER,
                     "failsafe state=%s action=%s",
                     receiver_failsafe_state_name(decision.state),
                     receiver_failsafe_action_name(decision.action));
        } else if ((decision.state == RECEIVER_FAILSAFE_LOSS_HOLD) ||
                   (decision.state == RECEIVER_FAILSAFE_STAGE_ONE)) {
            LOG_WARN(LOG_MODULE_RECEIVER,
                     "failsafe state=%s action=%s",
                     receiver_failsafe_state_name(decision.state),
                     receiver_failsafe_action_name(decision.action));
        } else {
            LOG_ERROR(LOG_MODULE_RECEIVER,
                      "failsafe state=%s action=%s",
                      receiver_failsafe_state_name(decision.state),
                      receiver_failsafe_action_name(decision.action));
        }
        logged_receiver_failsafe_state = decision.state;
    }

    if (decision.receiver_loss_detected &&
        !receiver_connection_fault_reported) {
        const fault_report_result_t report_result =
            fault_system_report(&firmware_fault_system,
                                FAULT_ID_RECEIVER_CONNECTION_LOST,
                                true,
                                (uint32_t)decision.state);

        firmware_fault_last_result = (uint32_t)report_result;
        receiver_connection_fault_reported =
            (report_result == FAULT_REPORT_RECORDED) ||
            (report_result == FAULT_REPORT_UPDATED);
    } else if (!decision.receiver_loss_detected &&
               receiver_connection_fault_reported) {
        const fault_clear_result_t clear_result =
            fault_system_clear(&firmware_fault_system,
                               FAULT_ID_RECEIVER_CONNECTION_LOST);

        if (clear_result == FAULT_CLEAR_OK) {
            receiver_connection_fault_reported = false;
        }
    }
}

static void diagnostic_fast_task(void *context)
{
    (void)context;
    firmware_fast_task_executions++;
}

static void diagnostic_medium_task(void *context)
{
    (void)context;
    firmware_medium_task_executions++;
}

static void diagnostic_slow_task(void *context)
{
    (void)context;
    firmware_slow_task_executions++;
}

static void usb_service_task(void *context)
{
    (void)context;

    usb_cdc_transport_process();
    firmware_usb_command_last_result =
        (uint32_t)usb_command_processor_process_once(
            &firmware_usb_command_processor);
    if (firmware_usb_command_processor.last_transition_valid) {
        firmware_system_state_last_result =
            (uint32_t)firmware_usb_command_processor.last_transition_result;
    }
    firmware_logging_drain_last_result =
        (uint32_t)logging_drain_once();
    firmware_usb_service_task_executions++;
}

static const task_definition_t diagnostic_task_definitions[] = {
    {
        .name = "diagnostic-fast",
        .period_us = 1000U,
        .priority = TASK_PRIORITY_HIGH,
        .callback = diagnostic_fast_task,
    },
    {
        .name = "diagnostic-medium",
        .period_us = 10000U,
        .priority = TASK_PRIORITY_NORMAL,
        .callback = diagnostic_medium_task,
    },
    {
        .name = "diagnostic-slow",
        .period_us = 100000U,
        .priority = TASK_PRIORITY_LOW,
        .callback = diagnostic_slow_task,
    },
};

static const task_definition_t usb_service_task_definition = {
    .name = "usb-service",
    .period_us = 1000U,
    .priority = TASK_PRIORITY_BACKGROUND,
    .callback = usb_service_task,
};

static const task_definition_t motor_control_task_definition = {
    .name = "motor-control",
    .period_us = MOTOR_CONTROL_FRAME_PERIOD_US,
    .priority = TASK_PRIORITY_HIGHEST,
    .callback = motor_control_task,
};

static const task_definition_t receiver_task_definition = {
    .name = "receiver-service",
    .period_us = RECEIVER_TASK_PERIOD_US,
    .priority = TASK_PRIORITY_HIGH,
    .callback = receiver_task,
    .context = &firmware_receiver_service,
};

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
    case BOARD_INIT_OK:
        break;
    }

    return FAULT_ID_MCU_INITIALIZATION;
}

static task_registration_result_t register_application_tasks(
    bool register_usb_service_task,
    bool register_receiver_task)
{
    size_t index;

    task_registry_initialize(&firmware_task_registry);

    {
        const task_registration_result_t result =
            task_registry_register(&firmware_task_registry,
                                   &motor_control_task_definition);

        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }

    if (register_receiver_task) {
        const task_registration_result_t result =
            task_registry_register(&firmware_task_registry,
                                   &receiver_task_definition);

        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }

    for (index = 0U;
         index < (sizeof(diagnostic_task_definitions) /
                  sizeof(diagnostic_task_definitions[0]));
         index++) {
        const task_registration_result_t result =
            task_registry_register(&firmware_task_registry,
                                   &diagnostic_task_definitions[index]);

        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }

    if (register_usb_service_task) {
        return task_registry_register(&firmware_task_registry,
                                      &usb_service_task_definition);
    }

    return TASK_REGISTRATION_OK;
}

int main(void)
{
    const fault_definition_t *fault_definitions;
    size_t fault_definition_count;
    board_init_result_t board_result;
    task_registration_result_t task_registration_result;
    scheduler_init_result_t scheduler_init_result;
    usb_cdc_init_result_t usb_init_result;
    motor_output_backend_t motor_output_backend;
    motor_control_init_result_t motor_control_result;
    receiver_source_t receiver_source;
    receiver_normalization_config_t receiver_normalization_config;
    receiver_freshness_config_t receiver_freshness_config;
    receiver_failsafe_config_t receiver_failsafe_config;
    board_receiver_init_result_t receiver_init_result;
    bool usb_service_available = false;
    bool receiver_service_available = false;

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
    if (!transition_system_state(
            SYSTEM_STATE_EVENT_INITIALIZATION_STARTED)) {
        stop_with_fault(BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR,
                        FAULT_ID_STATE_MACHINE_TRANSITION,
                        true,
                        (uint32_t)SYSTEM_STATE_EVENT_INITIALIZATION_STARTED);
    }

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZATION_STARTED;
    board_result = board_initialize();

    if (board_result != BOARD_INIT_OK) {
        stop_with_fault(boot_status_for_board_error(board_result),
                        fault_id_for_board_error(board_result),
                        true,
                        (uint32_t)board_result);
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
            (uint32_t)fault_system_report(
                &firmware_fault_system,
                FAULT_ID_LOGGING_CLOCK_ATTACHMENT,
                false,
                0U);
        LOG_ERROR(LOG_MODULE_SYSTEM, "logging clock attachment failed");
    }
    LOG_INFO(LOG_MODULE_BOARD, "Flight Computer V1 initialized");

    if (!dshot_motor_backend_prepare(&firmware_dshot_motor_backend,
                                     &motor_output_backend)) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        false,
                        0U);
    }
    motor_control_result = motor_control_initialize(
        &firmware_system_state_machine,
        &firmware_fault_system,
        time_us,
        MOTOR_COMMAND_DEFAULT_TIMEOUT_US,
        &motor_output_backend);
    firmware_motor_control_initialization_result =
        (uint32_t)motor_control_result;
    if (motor_control_result != MOTOR_CONTROL_INIT_OK) {
        stop_with_fault(BOOT_STATUS_MOTOR_INITIALIZATION_ERROR,
                        FAULT_ID_MOTOR_INITIALIZATION,
                        true,
                        (uint32_t)motor_control_result);
    }
    LOG_INFO(LOG_MODULE_SYSTEM, "four-channel DShot300 output initialized");

    receiver_init_result = board_receiver_initialize(&receiver_source);
    firmware_receiver_initialization_result = (uint32_t)receiver_init_result;
    if (receiver_init_result != BOARD_RECEIVER_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_RECEIVER_INITIALIZATION,
                                          true,
                                          (uint32_t)receiver_init_result);
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "receiver initialization failed result=%u",
                  (unsigned int)receiver_init_result);
    } else {
        receiver_normalization_default_config(
            &receiver_normalization_config);
        receiver_failsafe_default_config(&receiver_failsafe_config);
        receiver_freshness_config = (receiver_freshness_config_t){
            .fresh_through_us = receiver_failsafe_config.stale_after_us,
            .lost_after_us = receiver_failsafe_config.loss_detected_after_us,
        };
        if (receiver_service_initialize(&firmware_receiver_service,
                                        &receiver_source,
                                        time_us,
                                        &receiver_normalization_config,
                                        &receiver_freshness_config) &&
            receiver_failsafe_initialize(&firmware_receiver_failsafe,
                                         &receiver_failsafe_config,
                                         time_us())) {
            receiver_service_available = true;
            LOG_INFO(LOG_MODULE_RECEIVER,
                     "UART4 CRSF receiver initialized");
        } else {
            firmware_fault_last_result =
                (uint32_t)fault_system_report(
                    &firmware_fault_system,
                    FAULT_ID_RECEIVER_INITIALIZATION,
                    true,
                    (uint32_t)BOARD_RECEIVER_INIT_SOURCE_ERROR);
            LOG_ERROR(LOG_MODULE_RECEIVER,
                      "receiver service initialization failed");
        }
    }

    usb_init_result = usb_cdc_transport_initialize();
    firmware_usb_initialization_result = (uint32_t)usb_init_result;
    if (usb_init_result != USB_CDC_INIT_OK) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_USB_LOGGING_INITIALIZATION,
                                          true,
                                          (uint32_t)usb_init_result);
        LOG_ERROR(LOG_MODULE_USB,
                  "CDC initialization failed result=%u",
                  (unsigned int)usb_init_result);
    } else {
        const logging_backend_t backend = usb_logging_backend();

        if (logging_attach_backend(&backend) == LOGGING_BACKEND_ATTACH_OK) {
            if (usb_command_processor_initialize(
                    &firmware_usb_command_processor,
                    &firmware_system_state_machine,
                    &firmware_fault_system,
                    time_us,
                    firmware_version,
                    firmware_build_id) == USB_COMMAND_INIT_OK) {
                usb_service_available = true;
                LOG_INFO(LOG_MODULE_USB, "CDC JSON service initialized");
            } else {
                firmware_fault_last_result =
                    (uint32_t)fault_system_report(
                        &firmware_fault_system,
                        FAULT_ID_USB_COMMAND_INITIALIZATION,
                        false,
                        0U);
                LOG_ERROR(LOG_MODULE_USB,
                          "command processor initialization failed");
            }
        } else {
            firmware_fault_last_result =
                (uint32_t)fault_system_report(&firmware_fault_system,
                                              FAULT_ID_USB_LOGGING_INITIALIZATION,
                                              true,
                                              USB_LOGGING_FAULT_CONTEXT_BACKEND_ATTACHMENT);
            LOG_ERROR(LOG_MODULE_USB, "logging backend attachment failed");
        }
    }

    task_registration_result =
        register_application_tasks(usb_service_available,
                                   receiver_service_available);
    if (task_registration_result != TASK_REGISTRATION_OK) {
        stop_with_fault(BOOT_STATUS_TASK_REGISTRATION_ERROR,
                        FAULT_ID_TASK_REGISTRATION,
                        true,
                        (uint32_t)task_registration_result);
    }
    LOG_INFO(LOG_MODULE_TASK,
             "task registry initialized count=%u",
             (unsigned int)task_registry_count(&firmware_task_registry));
    scheduler_init_result = scheduler_initialize(&firmware_scheduler,
                                                 &firmware_task_registry,
                                                 time_us);
    if (scheduler_init_result != SCHEDULER_INIT_OK) {
        stop_with_fault(BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR,
                        FAULT_ID_SCHEDULER_INITIALIZATION,
                        true,
                        (uint32_t)scheduler_init_result);
    }
    LOG_INFO(LOG_MODULE_SCHEDULER, "scheduler initialized");
    if (!transition_system_state(
            SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED)) {
        stop_with_fault(BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR,
                        FAULT_ID_STATE_MACHINE_TRANSITION,
                        true,
                        (uint32_t)SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED);
    }
    LOG_INFO(LOG_MODULE_STATE, "INITIALIZING -> DISARMED");

    firmware_boot_status = BOOT_STATUS_RUNNING;
    LOG_INFO(LOG_MODULE_SYSTEM, "firmware running");

    for (;;) {
        const scheduler_step_result_t scheduler_result =
            scheduler_run_once(&firmware_scheduler);

        firmware_scheduler_last_result = (uint32_t)scheduler_result;
        if (scheduler_result == SCHEDULER_STEP_INVALID_STATE) {
            stop_with_fault(BOOT_STATUS_SCHEDULER_RUNTIME_ERROR,
                            FAULT_ID_SCHEDULER_RUNTIME,
                            true,
                            (uint32_t)scheduler_result);
        }

        firmware_uptime_us = time_us();
        firmware_main_loop_iterations++;
    }
}
