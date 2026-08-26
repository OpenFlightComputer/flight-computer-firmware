#include "boot_status.h"
#include "board.h"

volatile boot_status_t firmware_boot_status = BOOT_STATUS_RESET;
volatile uint32_t firmware_main_loop_iterations;

static void stop_with_status(boot_status_t status)
{
    firmware_boot_status = status;
    board_halt();
}

static boot_status_t boot_status_for_board_error(board_init_result_t result)
{
    switch (result) {
    case BOARD_INIT_MCU_ERROR:
        return BOOT_STATUS_MCU_INITIALIZATION_ERROR;
    case BOARD_INIT_CLOCK_CONFIGURATION_ERROR:
        return BOOT_STATUS_CLOCK_CONFIGURATION_ERROR;
    case BOARD_INIT_CLOCK_FREQUENCY_ERROR:
        return BOOT_STATUS_CLOCK_FREQUENCY_ERROR;
    case BOARD_INIT_OK:
        break;
    }

    return BOOT_STATUS_MCU_INITIALIZATION_ERROR;
}

int main(void)
{
    board_init_result_t board_result;

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZATION_STARTED;
    board_result = board_initialize();

    if (board_result != BOARD_INIT_OK) {
        stop_with_status(boot_status_for_board_error(board_result));
    }

    firmware_boot_status = BOOT_STATUS_BOARD_INITIALIZED;
    firmware_boot_status = BOOT_STATUS_RUNNING;

    for (;;) {
        firmware_main_loop_iterations++;
    }
}
