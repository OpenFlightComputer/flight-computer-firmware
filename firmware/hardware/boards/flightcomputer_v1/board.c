#include "board.h"

#include "board_definition.h"
#include "mcu.h"

board_init_result_t board_initialize(void)
{
    const mcu_init_result_t mcu_result = mcu_initialize();

    switch (mcu_result) {
    case MCU_INIT_HAL_ERROR:
        return BOARD_INIT_MCU_ERROR;
    case MCU_INIT_CLOCK_CONFIGURATION_ERROR:
        return BOARD_INIT_CLOCK_CONFIGURATION_ERROR;
    case MCU_INIT_OK:
        break;
    default:
        return BOARD_INIT_MCU_ERROR;
    }

    if (mcu_system_clock_frequency_hz() !=
        FLIGHTCOMPUTER_V1_SYSTEM_CLOCK_FREQUENCY_HZ) {
        return BOARD_INIT_CLOCK_FREQUENCY_ERROR;
    }

    return BOARD_INIT_OK;
}

_Noreturn void board_halt(void)
{
    mcu_halt();
}
