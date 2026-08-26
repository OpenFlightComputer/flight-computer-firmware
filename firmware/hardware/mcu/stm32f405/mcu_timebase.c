#include "mcu_timebase.h"

#include "timebase_snapshot.h"

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

static volatile uint32_t timebase_overflow_count;

static uint32_t tim5_input_clock_frequency_hz(void)
{
    uint32_t frequency_hz = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        frequency_hz *= 2U;
    }

    return frequency_hz;
}

mcu_timebase_init_result_t mcu_timebase_initialize(
    uint32_t expected_timer_clock_frequency_hz,
    uint32_t counter_frequency_hz,
    uint32_t interrupt_priority)
{
    uint32_t timer_clock_frequency_hz;
    uint32_t prescaler_divisor;

    if ((counter_frequency_hz == 0U) ||
        (expected_timer_clock_frequency_hz % counter_frequency_hz != 0U) ||
        (interrupt_priority >= (1UL << __NVIC_PRIO_BITS))) {
        return MCU_TIMEBASE_INIT_INVALID_CONFIGURATION;
    }

    timer_clock_frequency_hz = tim5_input_clock_frequency_hz();
    if (timer_clock_frequency_hz != expected_timer_clock_frequency_hz) {
        return MCU_TIMEBASE_INIT_CLOCK_FREQUENCY_ERROR;
    }

    prescaler_divisor = timer_clock_frequency_hz / counter_frequency_hz;
    if ((prescaler_divisor == 0U) || (prescaler_divisor > 65536U)) {
        return MCU_TIMEBASE_INIT_INVALID_CONFIGURATION;
    }

    __HAL_RCC_TIM5_CLK_ENABLE();

    TIM5->CR1 = 0U;
    TIM5->DIER = 0U;
    TIM5->PSC = prescaler_divisor - 1U;
    TIM5->ARR = UINT32_MAX;
    TIM5->CNT = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;

    timebase_overflow_count = 0U;
    NVIC_ClearPendingIRQ(TIM5_IRQn);
    NVIC_SetPriority(TIM5_IRQn, interrupt_priority);
    NVIC_EnableIRQ(TIM5_IRQn);

    TIM5->DIER = TIM_DIER_UIE;
    TIM5->CR1 = TIM_CR1_CEN;

    return MCU_TIMEBASE_INIT_OK;
}

uint64_t mcu_timebase_us(void)
{
    timebase_snapshot_t snapshot;
    uint64_t time_value_us;

    do {
        snapshot.overflow_before = timebase_overflow_count;
        snapshot.counter_before = TIM5->CNT;
        snapshot.update_pending = (TIM5->SR & TIM_SR_UIF) != 0U;
        snapshot.counter_after = TIM5->CNT;
        snapshot.overflow_after = timebase_overflow_count;
    } while (!timebase_snapshot_resolve(&snapshot, &time_value_us));

    return time_value_us;
}

void mcu_timebase_handle_overflow_interrupt(void)
{
    if ((TIM5->SR & TIM_SR_UIF) != 0U) {
        TIM5->SR = 0U;
        timebase_overflow_count++;
    }
}
