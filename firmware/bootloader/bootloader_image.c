#include "bootloader_image.h"

#include "bootloader_contract.h"
#include "bootloader_crc32.h"
#include "stm32f4xx.h"

#define SRAM_START UINT32_C(0x20000000)
#define SRAM_END UINT32_C(0x20020000)
#define CCMRAM_START UINT32_C(0x10000000)
#define CCMRAM_END UINT32_C(0x10010000)
#define NVIC_REGISTER_COUNT 8U

static bool stack_pointer_is_valid(uint32_t stack_pointer)
{
    return ((stack_pointer >= SRAM_START) && (stack_pointer <= SRAM_END)) ||
           ((stack_pointer >= CCMRAM_START) && (stack_pointer <= CCMRAM_END));
}

static bool reset_handler_is_valid(uint32_t reset_handler)
{
    const uint32_t address = reset_handler & ~UINT32_C(1);

    return ((reset_handler & 1U) != 0U) &&
           (address >= OFC_APPLICATION_FLASH_START) &&
           (address < OFC_APPLICATION_METADATA_ADDRESS);
}

__attribute__((naked, noreturn)) static void jump_with_application_stack(
    uint32_t stack_pointer __attribute__((unused)),
    uint32_t reset_handler __attribute__((unused)))
{
    __asm volatile(
        "msr msp, r0\n"
        "cpsie i\n"
        "bx r1\n");
}

bool bootloader_application_is_valid(void)
{
    const ofc_application_metadata_t *metadata =
        (const ofc_application_metadata_t *)OFC_APPLICATION_METADATA_ADDRESS;
    const uint32_t *vectors =
        (const uint32_t *)OFC_APPLICATION_FLASH_START;

    if ((metadata->magic != OFC_APPLICATION_METADATA_MAGIC) ||
        (metadata->format_version != OFC_APPLICATION_METADATA_VERSION) ||
        (metadata->image_length < 8U) ||
        (metadata->image_length > OFC_APPLICATION_MAXIMUM_LENGTH) ||
        !stack_pointer_is_valid(vectors[0]) ||
        !reset_handler_is_valid(vectors[1])) {
        return false;
    }
    return bootloader_crc32((const void *)OFC_APPLICATION_FLASH_START,
                            metadata->image_length) ==
           metadata->image_crc32;
}

_Noreturn void bootloader_start_application(void)
{
    const uint32_t *vectors =
        (const uint32_t *)OFC_APPLICATION_FLASH_START;
    const uint32_t stack_pointer = vectors[0];
    const uint32_t reset_handler = vectors[1];
    uint32_t index;

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (index = 0U; index < NVIC_REGISTER_COUNT; ++index) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }
    SCB->VTOR = OFC_APPLICATION_FLASH_START;
    __set_CONTROL(0U);
    __DSB();
    __ISB();
    jump_with_application_stack(stack_pointer, reset_handler);
}
