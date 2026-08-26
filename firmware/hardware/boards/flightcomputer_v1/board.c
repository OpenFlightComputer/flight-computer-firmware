#include "board.h"

#include "board_definition.h"
#include "mcu.h"
#include "mcu_timebase.h"

board_init_result_t board_initialize(void)
{
    const mcu_init_result_t mcu_result = mcu_initialize();
    mcu_timebase_init_result_t timebase_result;

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

    timebase_result = mcu_timebase_initialize(
        FLIGHTCOMPUTER_V1_TIMEBASE_TIMER_CLOCK_FREQUENCY_HZ,
        FLIGHTCOMPUTER_V1_TIMEBASE_COUNTER_FREQUENCY_HZ,
        FLIGHTCOMPUTER_V1_TIMEBASE_INTERRUPT_PRIORITY);
    if (timebase_result != MCU_TIMEBASE_INIT_OK) {
        return BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR;
    }

    return BOARD_INIT_OK;
}

_Noreturn void board_halt(void)
{
    mcu_halt();
}
