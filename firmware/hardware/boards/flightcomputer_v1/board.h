#ifndef OPENFLIGHTCOMPUTER_BOARD_H
#define OPENFLIGHTCOMPUTER_BOARD_H

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

#endif
