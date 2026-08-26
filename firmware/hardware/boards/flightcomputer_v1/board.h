#ifndef OPENFLIGHTCOMPUTER_BOARD_H
#define OPENFLIGHTCOMPUTER_BOARD_H

typedef enum {
    BOARD_INIT_OK = 0,
    BOARD_INIT_MCU_ERROR,
    BOARD_INIT_CLOCK_CONFIGURATION_ERROR,
    BOARD_INIT_CLOCK_FREQUENCY_ERROR,
    BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR,
} board_init_result_t;

board_init_result_t board_initialize(void);
_Noreturn void board_halt(void);

#endif
