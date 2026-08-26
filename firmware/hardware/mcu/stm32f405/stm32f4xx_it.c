#include "stm32f4xx_it.h"

#include "mcu_timebase.h"

#include "stm32f4xx_hal.h"

void NMI_Handler(void)
{
    for (;;) {
    }
}

void HardFault_Handler(void)
{
    for (;;) {
    }
}

void MemManage_Handler(void)
{
    for (;;) {
    }
}

void BusFault_Handler(void)
{
    for (;;) {
    }
}

void UsageFault_Handler(void)
{
    for (;;) {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void TIM5_IRQHandler(void)
{
    mcu_timebase_handle_overflow_interrupt();
}
