#include "board.h"

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#define BOARD_BOOTLOADER_REQUEST_MAGIC UINT32_C(0x4F464344)
#define BOARD_SYSTEM_MEMORY_BASE UINT32_C(0x1FFF0000)
#define BOARD_SYSTEM_MEMORY_END UINT32_C(0x1FFF7800)
#define BOARD_SRAM_BASE UINT32_C(0x20000000)
#define BOARD_SRAM_END UINT32_C(0x20020000)
#define BOARD_NVIC_REGISTER_COUNT 8U

static void enable_backup_domain_access(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
}

static bool system_memory_vectors_are_valid(uint32_t stack_pointer,
                                            uint32_t reset_handler)
{
    const uint32_t handler_address = reset_handler & ~UINT32_C(1);

    return (stack_pointer >= BOARD_SRAM_BASE) &&
           (stack_pointer < BOARD_SRAM_END) &&
           ((reset_handler & UINT32_C(1)) != 0U) &&
           (handler_address >= BOARD_SYSTEM_MEMORY_BASE) &&
           (handler_address < BOARD_SYSTEM_MEMORY_END);
}

static _Noreturn void enter_system_memory(void)
{
    const uint32_t stack_pointer =
        *(volatile const uint32_t *)BOARD_SYSTEM_MEMORY_BASE;
    const uint32_t reset_handler =
        *(volatile const uint32_t *)(BOARD_SYSTEM_MEMORY_BASE + 4U);
    uint32_t index;

    if (!system_memory_vectors_are_valid(stack_pointer, reset_handler)) {
        NVIC_SystemReset();
    }

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (index = 0U; index < BOARD_NVIC_REGISTER_COUNT; ++index) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    SCB->VTOR = 0U;
    __DSB();
    __ISB();

    __set_CONTROL(0U);
    __set_MSP(stack_pointer);
    __DSB();
    __ISB();
    ((void (*)(void))reset_handler)();

    for (;;) {
        __NOP();
    }
}

void board_enter_usb_bootloader_if_requested(void)
{
    const uint32_t previous_apb1enr = RCC->APB1ENR;
    uint32_t previous_pwr_cr;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    previous_pwr_cr = PWR->CR;
    enable_backup_domain_access();
    if (RTC->BKP0R != BOARD_BOOTLOADER_REQUEST_MAGIC) {
        PWR->CR = previous_pwr_cr;
        RCC->APB1ENR = previous_apb1enr;
        return;
    }

    RTC->BKP0R = 0U;
    __DSB();
    enter_system_memory();
}

_Noreturn void board_request_usb_bootloader(void)
{
    __disable_irq();
    enable_backup_domain_access();
    RTC->BKP0R = BOARD_BOOTLOADER_REQUEST_MAGIC;
    __DSB();
    NVIC_SystemReset();
}
