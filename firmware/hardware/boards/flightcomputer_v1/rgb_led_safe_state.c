#include "rgb_led_safe_state.h"

#include "board_definition.h"

#include "stm32f4xx_hal.h"

#include <stddef.h>

/* Physically accepted V1 timing, used only for one all-zero startup frame. */
#define WS2812_DATA_BIT_COUNT 24U
#define WS2812_RESET_TIME_US 1000U
#define WS2812_ZERO_HIGH_CYCLES 48U
#define WS2812_ZERO_LOW_CYCLES 150U

__STATIC_FORCEINLINE void wait_cycles(uint32_t cycles)
{
    const uint32_t started_at = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - started_at) < cycles) {
        __NOP();
    }
}

__STATIC_FORCEINLINE void drive_data_high(void)
{
    GPIOA->BSRR = GPIO_PIN_1;
    __DSB();
}

__STATIC_FORCEINLINE void drive_data_low(void)
{
    GPIOA->BSRR = (uint32_t)GPIO_PIN_1 << 16U;
    __DSB();
}

static void wait_reset_interval(uint32_t core_clock_hz)
{
    wait_cycles((core_clock_hz / 1000000U) * WS2812_RESET_TIME_US);
}

void flightcomputer_v1_rgb_led_force_off(uint32_t core_clock_hz)
{
    GPIO_InitTypeDef pin = {0};
    uint32_t previous_primask;
    size_t bit_index;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Preload low before changing MODER so enabling the output cannot glitch. */
    drive_data_low();
    pin.Pin = GPIO_PIN_1;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &pin);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* The board has already rejected any unexpected system-clock frequency. */
    if ((core_clock_hz != FLIGHTCOMPUTER_V1_SYSTEM_CLOCK_FREQUENCY_HZ) ||
        ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)) {
        return;
    }

    wait_reset_interval(core_clock_hz);

    /*
     * Twenty-four logical-zero bits are GRB=(0,0,0). Preserve PRIMASK so the
     * boot-only operation cannot accidentally enable previously masked IRQs.
     */
    previous_primask = __get_PRIMASK();
    __disable_irq();
    for (bit_index = 0U; bit_index < WS2812_DATA_BIT_COUNT; bit_index++) {
        drive_data_high();
        wait_cycles(WS2812_ZERO_HIGH_CYCLES);
        drive_data_low();
        wait_cycles(WS2812_ZERO_LOW_CYCLES);
    }
    __set_PRIMASK(previous_primask);

    wait_reset_interval(core_clock_hz);
}
