#ifndef OPENFLIGHTCOMPUTER_BOARD_H
#define OPENFLIGHTCOMPUTER_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BOARD_INIT_OK = 0,
    BOARD_INIT_MCU_ERROR,
    BOARD_INIT_CLOCK_CONFIGURATION_ERROR,
    BOARD_INIT_CLOCK_FREQUENCY_ERROR,
    BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR,
} board_init_result_t;

typedef enum {
    BOARD_MOTOR_OUTPUT_INIT_OK = 0,
    BOARD_MOTOR_OUTPUT_INIT_INVALID_ARGUMENT,
    BOARD_MOTOR_OUTPUT_INIT_CLOCK_ERROR,
    BOARD_MOTOR_OUTPUT_INIT_HARDWARE_ERROR,
} board_motor_output_init_result_t;

typedef enum {
    BOARD_MOTOR_OUTPUT_SUBMIT_ACCEPTED = 0,
    BOARD_MOTOR_OUTPUT_SUBMIT_BUSY,
    BOARD_MOTOR_OUTPUT_SUBMIT_ERROR,
} board_motor_output_submit_result_t;

typedef enum {
    BOARD_MOTOR_OUTPUT_STOP_ACCEPTED = 0,
    BOARD_MOTOR_OUTPUT_STOP_ERROR,
} board_motor_output_stop_result_t;

typedef enum {
    BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED = 0,
    BOARD_MOTOR_OUTPUT_STATUS_IDLE,
    BOARD_MOTOR_OUTPUT_STATUS_ACTIVE,
    BOARD_MOTOR_OUTPUT_STATUS_ERROR,
} board_motor_output_status_t;

typedef enum {
    BOARD_MOTOR_DIAGNOSTIC_NONE = 0,
    BOARD_MOTOR_DIAGNOSTIC_INVALID_TABLE = 101,
    BOARD_MOTOR_DIAGNOSTIC_REORDER_FAILED = 102,
    BOARD_MOTOR_DIAGNOSTIC_DMA_DISABLE_TIMEOUT = 103,
    BOARD_MOTOR_DIAGNOSTIC_TRANSFER_START_FAILED = 104,
    BOARD_MOTOR_DIAGNOSTIC_STOP_TRANSFER_TIMEOUT = 105,
    BOARD_MOTOR_DIAGNOSTIC_DMA_FIFO_ERROR = 106,
    BOARD_MOTOR_DIAGNOSTIC_DMA_DIRECT_MODE_ERROR = 107,
    BOARD_MOTOR_DIAGNOSTIC_DMA_TRANSFER_ERROR = 108,
    BOARD_MOTOR_DIAGNOSTIC_IRQ_MISSING_COMPLETION = 109,
    BOARD_MOTOR_DIAGNOSTIC_IRQ_UNEXPECTED_STATE = 110,
} board_motor_output_diagnostic_reason_t;

board_init_result_t board_initialize(void);
_Noreturn void board_halt(void);

uint32_t board_motor_output_timer_clock_frequency_hz(void);
board_motor_output_init_result_t board_motor_output_initialize(
    uint16_t bit_period_ticks);
/* Each consecutive group of four values is ordered ESC_M1 through ESC_M4. */
board_motor_output_submit_result_t board_motor_output_submit(
    const uint16_t *compare_values,
    size_t compare_value_count);
/* The stop table uses the same physical ESC_M1-through-ESC_M4 column order. */
board_motor_output_stop_result_t board_motor_output_force_stop(
    const uint16_t *stop_compare_values,
    size_t compare_value_count);
board_motor_output_status_t board_motor_output_status(void);
uint32_t board_motor_output_error_context(void);

#endif
