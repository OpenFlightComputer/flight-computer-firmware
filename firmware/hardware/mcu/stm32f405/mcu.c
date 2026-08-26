#include "mcu.h"

#include "system_clock.h"

#include "stm32f4xx_hal.h"

mcu_init_result_t mcu_initialize(void)
{
    if (HAL_Init() != HAL_OK) {
        return MCU_INIT_HAL_ERROR;
    }
    if (system_clock_configure() != HAL_OK) {
        return MCU_INIT_CLOCK_CONFIGURATION_ERROR;
    }

    return MCU_INIT_OK;
}

uint32_t mcu_system_clock_frequency_hz(void)
{
    return SystemCoreClock;
}

_Noreturn void mcu_halt(void)
{
    __disable_irq();

    for (;;) {
        __NOP();
    }
}
