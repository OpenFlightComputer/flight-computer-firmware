#include "rgb_led.h"

#include "board_definition.h"

#include "stm32f4xx_hal.h"

/* Physically accepted V1 WS2812 timing copied from the manufacturing tester. */
#define WS2812_RESET_TIME_US 1000U
#define WS2812_ZERO_HIGH_CYCLES 48U
#define WS2812_ZERO_LOW_CYCLES 150U
#define WS2812_ONE_HIGH_CYCLES 105U
#define WS2812_ONE_LOW_CYCLES 105U

static uint32_t rgb_core_clock_hz;
static bool rgb_initialized;

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

static void send_byte(uint8_t value)
{
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U) {
        drive_data_high();
        if ((value & mask) != 0U) {
            wait_cycles(WS2812_ONE_HIGH_CYCLES);
            drive_data_low();
            wait_cycles(WS2812_ONE_LOW_CYCLES);
        } else {
            wait_cycles(WS2812_ZERO_HIGH_CYCLES);
            drive_data_low();
            wait_cycles(WS2812_ZERO_LOW_CYCLES);
        }
    }
}

bool flightcomputer_v1_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t previous_primask;

    if (!rgb_initialized) {
        return false;
    }
    wait_reset_interval(rgb_core_clock_hz);
    previous_primask = __get_PRIMASK();
    __disable_irq();
    send_byte(green);
    send_byte(red);
    send_byte(blue);
    __set_PRIMASK(previous_primask);
    wait_reset_interval(rgb_core_clock_hz);
    return true;
}

bool flightcomputer_v1_rgb_led_initialize(uint32_t core_clock_hz)
{
    GPIO_InitTypeDef pin = {0};

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
        return false;
    }
    rgb_core_clock_hz = core_clock_hz;
    rgb_initialized = true;
    return flightcomputer_v1_rgb_led_set(0U, 0U, 0U);
}
