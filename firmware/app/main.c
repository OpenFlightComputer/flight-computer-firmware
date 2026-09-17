#include "application_runtime.h"
#include "bootloader_contract.h"

#include "stm32f4xx.h"

int main(void)
{
    SCB->VTOR = OFC_APPLICATION_FLASH_START;
    __DSB();
    __ISB();
    application_runtime_initialize();
    application_runtime_run();
    return 0;
}
