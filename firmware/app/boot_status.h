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
} boot_status_t;

extern volatile boot_status_t firmware_boot_status;
extern volatile uint32_t firmware_main_loop_iterations;
extern volatile uint64_t firmware_uptime_us;

#endif
