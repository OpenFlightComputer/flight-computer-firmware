#ifndef OPENFLIGHTCOMPUTER_MCU_H
#define OPENFLIGHTCOMPUTER_MCU_H

#include <stdint.h>

typedef enum {
    MCU_INIT_OK = 0,
    MCU_INIT_HAL_ERROR,
    MCU_INIT_CLOCK_CONFIGURATION_ERROR,
} mcu_init_result_t;

mcu_init_result_t mcu_initialize(void);
uint32_t mcu_system_clock_frequency_hz(void);
_Noreturn void mcu_halt(void);

#endif
