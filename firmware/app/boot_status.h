#ifndef OPENFLIGHTCOMPUTER_BOOT_STATUS_H
#define OPENFLIGHTCOMPUTER_BOOT_STATUS_H

#include <stdint.h>

typedef enum {
    BOOT_STATUS_RESET = 0U,
    BOOT_STATUS_BOARD_INITIALIZATION_STARTED = 1U,
    BOOT_STATUS_BOARD_INITIALIZED = 2U,
    BOOT_STATUS_RUNNING = 3U,
    BOOT_STATUS_MCU_INITIALIZATION_ERROR = 100U,
    BOOT_STATUS_CLOCK_CONFIGURATION_ERROR = 101U,
    BOOT_STATUS_CLOCK_FREQUENCY_ERROR = 102U,
    BOOT_STATUS_TIMEBASE_CONFIGURATION_ERROR = 103U,
    BOOT_STATUS_TASK_REGISTRATION_ERROR = 104U,
    BOOT_STATUS_SCHEDULER_INITIALIZATION_ERROR = 105U,
    BOOT_STATUS_SCHEDULER_RUNTIME_ERROR = 106U,
    BOOT_STATUS_STATE_MACHINE_TRANSITION_ERROR = 107U,
    BOOT_STATUS_FAULT_SYSTEM_INITIALIZATION_ERROR = 108U,
    BOOT_STATUS_FAULT_CLOCK_ATTACHMENT_ERROR = 109U,
} boot_status_t;

extern volatile boot_status_t firmware_boot_status;
extern volatile uint32_t firmware_main_loop_iterations;
extern volatile uint64_t firmware_uptime_us;
extern volatile uint32_t firmware_scheduler_last_result;
extern volatile uint32_t firmware_fast_task_executions;
extern volatile uint32_t firmware_medium_task_executions;
extern volatile uint32_t firmware_slow_task_executions;
extern volatile uint32_t firmware_system_state_last_result;
extern volatile uint32_t firmware_fault_last_result;
extern volatile uint32_t firmware_usb_initialization_result;
extern volatile uint32_t firmware_logging_drain_last_result;
extern volatile uint32_t firmware_usb_command_last_result;
extern volatile uint32_t firmware_usb_service_task_executions;

#endif
