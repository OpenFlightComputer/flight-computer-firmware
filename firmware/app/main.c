#include "boot_status.h"
#include "board.h"
#include "fault.h"
#include "fault_catalog.h"
#include "logging.h"
#include "scheduler.h"
#include "system_state.h"
#include "task.h"
#include "time.h"

#include <stddef.h>

volatile boot_status_t firmware_boot_status = BOOT_STATUS_RESET;
volatile uint32_t firmware_main_loop_iterations;
volatile uint64_t firmware_uptime_us;
volatile uint32_t firmware_scheduler_last_result;
volatile uint32_t firmware_fast_task_executions;
volatile uint32_t firmware_medium_task_executions;
volatile uint32_t firmware_slow_task_executions;
volatile uint32_t firmware_system_state_last_result;
volatile uint32_t firmware_fault_last_result = UINT32_MAX;

task_registry_t firmware_task_registry;
scheduler_t firmware_scheduler;
system_state_machine_t firmware_system_state_machine;
fault_system_t firmware_fault_system;

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

static task_registration_result_t register_diagnostic_tasks(void)
{
    size_t index;

    task_registry_initialize(&firmware_task_registry);

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

    return TASK_REGISTRATION_OK;
}

int main(void)
{
    const fault_definition_t *fault_definitions;
    size_t fault_definition_count;
    board_init_result_t board_result;
    task_registration_result_t task_registration_result;
    scheduler_init_result_t scheduler_init_result;

    logging_initialize();
    LOG_INFO(LOG_MODULE_SYSTEM, "OpenFlightComputer booting");

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

    task_registration_result = register_diagnostic_tasks();
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
