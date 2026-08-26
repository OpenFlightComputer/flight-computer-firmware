#include "boot_status.h"
#include "system_clock.h"

#include "stm32f4xx_hal.h"

volatile boot_status_t firmware_boot_status = BOOT_STATUS_RESET;
volatile uint32_t firmware_main_loop_iterations;

static void stop_with_status(boot_status_t status)
{
    firmware_boot_status = status;
    __disable_irq();

    for (;;) {
        __NOP();
    }
}

int main(void)
{
    HAL_Init();
    firmware_boot_status = BOOT_STATUS_HAL_INITIALIZED;

    if (system_clock_configure() != HAL_OK) {
        stop_with_status(BOOT_STATUS_CLOCK_CONFIGURATION_ERROR);
    }

    firmware_boot_status = BOOT_STATUS_CLOCK_CONFIGURED;
    if (SystemCoreClock != SYSTEM_CLOCK_FREQUENCY_HZ) {
        stop_with_status(BOOT_STATUS_CLOCK_FREQUENCY_ERROR);
    }

    firmware_boot_status = BOOT_STATUS_RUNNING;

    for (;;) {
        firmware_main_loop_iterations++;
    }
}
