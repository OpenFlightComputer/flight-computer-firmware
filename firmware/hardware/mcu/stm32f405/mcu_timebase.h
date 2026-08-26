#ifndef OPENFLIGHTCOMPUTER_MCU_TIMEBASE_H
#define OPENFLIGHTCOMPUTER_MCU_TIMEBASE_H

#include <stdint.h>

typedef enum {
    MCU_TIMEBASE_INIT_OK = 0,
    MCU_TIMEBASE_INIT_INVALID_CONFIGURATION,
    MCU_TIMEBASE_INIT_CLOCK_FREQUENCY_ERROR,
} mcu_timebase_init_result_t;

mcu_timebase_init_result_t mcu_timebase_initialize(
    uint32_t expected_timer_clock_frequency_hz,
    uint32_t counter_frequency_hz,
    uint32_t interrupt_priority);
uint64_t mcu_timebase_us(void);
void mcu_timebase_handle_overflow_interrupt(void);

#endif
