#include "board.h"

#include "bootloader_contract.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

static void enable_backup_domain_access(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
}

_Noreturn void board_request_usb_bootloader(void)
{
    __disable_irq();
    enable_backup_domain_access();
    RTC->BKP0R = OFC_BOOTLOADER_REQUEST_MAGIC;
    __DSB();
    NVIC_SystemReset();
}
