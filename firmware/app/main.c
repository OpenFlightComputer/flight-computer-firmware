#include "boot_status.h"
#include "board.h"
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

task_registry_t firmware_task_registry;
scheduler_t firmware_scheduler;
system_state_machine_t firmware_system_state_machine;

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

static void stop_with_status(boot_status_t status)
{
    firmware_system_state_last_result =
        (uint32_t)system_state_machine_handle_event(
            &firmware_system_state_machine,
            SYSTEM_STATE_EVENT_FAULT_DETECTED);
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

static bool register_diagnostic_tasks(void)
{
    size_t index;

    task_registry_initialize(&firmware_task_registry);

    for (index = 0U;
         index < (sizeof(diagnostic_task_definitions) /
                  sizeof(diagnostic_task_definitions[0]));
         index++) {
        if (task_registry_register(&firmware_task_registry,
                                   &diagnostic_task_definitions[index]) !=
            TASK_REGISTRATION_OK) {
            return false;
        }
    }

    return true;
}

int main(void)
{
    board_init_result_t board_result;

    system_state_machine_initialize(&firmware_system_state_machine);
    if (!transition_system_state(
            SYSTEM_STATE_EVENT_INITIALIZATION_STARTED)) {
        stop_with_status(BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR);
    }

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZATION_STARTED;
    board_result = board_initialize();

    if (board_result != BOARD_INIT_OK) {
        stop_with_status(boot_status_for_board_error(board_result));
    }

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZED;

    if (!register_diagnostic_tasks()) {
        stop_with_status(BOOT_STATUS_TASK_REGISTRATION_ERROR);
    }
    if (scheduler_initialize(&firmware_scheduler,
                             &firmware_task_registry,
                             time_us) != SCHEDULER_INIT_OK) {
        stop_with_status(BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR);
    }
    if (!transition_system_state(
            SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED)) {
        stop_with_status(BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR);
    }

    firmware_boot_status = BOOT_STATUS_RUNNING;

    for (;;) {
        const scheduler_step_result_t scheduler_result =
            scheduler_run_once(&firmware_scheduler);

        firmware_scheduler_last_result = (uint32_t)scheduler_result;
        if (scheduler_result == SCHEDULER_STEP_INVALID_STATE) {
            stop_with_status(BOOT_STATUS_SCHEDULER_RUNTIME_ERROR);
        }

        firmware_uptime_us = time_us();
        firmware_main_loop_iterations++;
    }
}
