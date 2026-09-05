#include "board.h"

#include "board_definition.h"
#include "motor_output_map.h"

#include "stm32f4xx_hal.h"

#include <stddef.h>
#include <stdint.h>

#define MOTOR_GPIO_FIRST_PIN 6U
#define MOTOR_GPIO_LAST_PIN 9U
#define MOTOR_GPIO_MODE_OUTPUT 1U
#define MOTOR_GPIO_MODE_ALTERNATE 2U
#define MOTOR_TIMER_DMA_BASE_CCR1 13U
#define MOTOR_TIMER_DMA_BURST_LENGTH_MINUS_ONE 3U
#define MOTOR_DMA_DISABLE_WAIT_LIMIT 100000U
#define MOTOR_STOP_TRANSFER_WAIT_LIMIT 1000000U

#define MOTOR_DMA_STREAM_FLAGS                                              \
    (DMA_LIFCR_CFEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CTEIF1 |            \
     DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTCIF1)
#define MOTOR_DMA_ERROR_FLAGS                                               \
    (DMA_LISR_FEIF1 | DMA_LISR_DMEIF1 | DMA_LISR_TEIF1)

static volatile board_motor_output_status_t board_output_state =
    BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED;
static uint16_t motor_bit_period_ticks;
static uint16_t motor_dma_compare_values[
    FLIGHTCOMPUTER_V1_MOTOR_DMA_COMPARE_VALUE_CAPACITY];

_Static_assert(offsetof(TIM_TypeDef, CCR1) / sizeof(uint32_t) ==
                   MOTOR_TIMER_DMA_BASE_CCR1,
               "TIM8 DMA base must select CCR1");
_Static_assert(
    (FLIGHTCOMPUTER_V1_MOTOR_DMA_COMPARE_VALUE_CAPACITY %
     FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT) == 0U,
    "The motor DMA buffer must contain complete four-output rows");

static uint32_t tim8_input_clock_frequency_hz(void)
{
    uint32_t frequency_hz = HAL_RCC_GetPCLK2Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1) {
        frequency_hz *= 2U;
    }

    return frequency_hz;
}

static uint32_t motor_pin_mask(void)
{
    uint32_t mask = 0U;
    size_t physical_output;

    for (physical_output = 0U;
         physical_output < FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT;
         physical_output++) {
        const flightcomputer_v1_motor_output_route_t *route =
            flightcomputer_v1_motor_output_route(physical_output);

        if (route != NULL) {
            mask |= UINT32_C(1) << route->gpio_pin;
        }
    }

    return mask;
}

static bool routes_are_valid(void)
{
    uint32_t channels_seen = 0U;
    size_t physical_output;

    for (physical_output = 0U;
         physical_output < FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT;
         physical_output++) {
        const flightcomputer_v1_motor_output_route_t *route =
            flightcomputer_v1_motor_output_route(physical_output);

        if ((route == NULL) ||
            (route->gpio_port != FLIGHTCOMPUTER_V1_GPIO_PORT_C) ||
            (route->gpio_pin < MOTOR_GPIO_FIRST_PIN) ||
            (route->gpio_pin > MOTOR_GPIO_LAST_PIN) ||
            (route->gpio_alternate_function !=
             FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION) ||
            (route->timer_channel < 1U) ||
            (route->timer_channel > FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT)) {
            return false;
        }

        channels_seen |= UINT32_C(1) << (route->timer_channel - 1U);
    }

    return channels_seen ==
           ((UINT32_C(1) << FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT) - 1U);
}

static void set_motor_gpio_mode(uint32_t mode)
{
    uint32_t clear_mask = 0U;
    uint32_t set_mask = 0U;
    size_t physical_output;

    for (physical_output = 0U;
         physical_output < FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT;
         physical_output++) {
        const flightcomputer_v1_motor_output_route_t *route =
            flightcomputer_v1_motor_output_route(physical_output);
        const uint32_t shift = (uint32_t)route->gpio_pin * 2U;

        clear_mask |= UINT32_C(3) << shift;
        set_mask |= mode << shift;
    }

    GPIOC->MODER = (GPIOC->MODER & ~clear_mask) | set_mask;
}

static void drive_motor_pins_low(void)
{
    GPIOC->BSRR = motor_pin_mask() << 16U;
    set_motor_gpio_mode(MOTOR_GPIO_MODE_OUTPUT);
}

static bool disable_dma_stream(void)
{
    uint32_t remaining = MOTOR_DMA_DISABLE_WAIT_LIMIT;

    DMA2_Stream1->CR &= ~DMA_SxCR_EN;
    while (((DMA2_Stream1->CR & DMA_SxCR_EN) != 0U) &&
           (remaining > 0U)) {
        remaining--;
    }

    return (DMA2_Stream1->CR & DMA_SxCR_EN) == 0U;
}

static void clear_timer_compare_values(void)
{
    TIM8->CCR1 = 0U;
    TIM8->CCR2 = 0U;
    TIM8->CCR3 = 0U;
    TIM8->CCR4 = 0U;
    TIM8->EGR = TIM_EGR_UG;
    TIM8->SR = 0U;
}

static bool stop_transfer_hardware(void)
{
    bool dma_disabled;

    TIM8->DIER &= ~TIM_DIER_UDE;
    TIM8->CR1 &= ~TIM_CR1_CEN;
    dma_disabled = disable_dma_stream();
    clear_timer_compare_values();
    drive_motor_pins_low();
    DMA2->LIFCR = MOTOR_DMA_STREAM_FLAGS;
    return dma_disabled;
}

static bool compare_values_are_valid(const uint16_t *compare_values,
                                     size_t compare_value_count)
{
    size_t index;

    if ((compare_values == NULL) || (compare_value_count == 0U) ||
        (compare_value_count >
         FLIGHTCOMPUTER_V1_MOTOR_DMA_COMPARE_VALUE_CAPACITY) ||
        ((compare_value_count % FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT) != 0U) ||
        ((((uintptr_t)compare_values) & (sizeof(uint16_t) - 1U)) != 0U)) {
        return false;
    }

    for (index = 0U; index < compare_value_count; index++) {
        if (compare_values[index] > motor_bit_period_ticks) {
            return false;
        }
    }

    return true;
}

static bool copy_physical_values_to_dma_order(
    const uint16_t *physical_compare_values,
    size_t compare_value_count)
{
    size_t row_offset;

    for (row_offset = 0U;
         row_offset < compare_value_count;
         row_offset += FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT) {
        if (!flightcomputer_v1_motor_output_order_compare_row(
                &physical_compare_values[row_offset],
                &motor_dma_compare_values[row_offset])) {
            return false;
        }
    }

    return true;
}

static bool start_transfer(const uint16_t *compare_values,
                           size_t compare_value_count,
                           bool interrupt_enabled)
{
    const flightcomputer_v1_motor_output_group_t *group =
        flightcomputer_v1_motor_output_group();
    uint32_t dma_configuration;

    if (!stop_transfer_hardware()) {
        return false;
    }

    dma_configuration =
        ((uint32_t)group->dma_channel << DMA_SxCR_CHSEL_Pos) |
        DMA_SxCR_PL | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 |
        DMA_SxCR_MINC | DMA_SxCR_DIR_0;
    if (interrupt_enabled) {
        dma_configuration |=
            DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    }

    DMA2_Stream1->CR = dma_configuration;
    DMA2_Stream1->NDTR = (uint32_t)compare_value_count;
    DMA2_Stream1->PAR = (uint32_t)(uintptr_t)&TIM8->DMAR;
    DMA2_Stream1->M0AR = (uint32_t)(uintptr_t)compare_values;
    DMA2_Stream1->FCR = interrupt_enabled ? DMA_SxFCR_FEIE : 0U;

    TIM8->CNT = 0U;
    TIM8->SR = 0U;
    set_motor_gpio_mode(MOTOR_GPIO_MODE_ALTERNATE);
    DMA2_Stream1->CR |= DMA_SxCR_EN;
    TIM8->DIER |= TIM_DIER_UDE;
    TIM8->CR1 |= TIM_CR1_CEN;
    return true;
}

uint32_t board_motor_output_timer_clock_frequency_hz(void)
{
    return FLIGHTCOMPUTER_V1_MOTOR_TIMER_CLOCK_FREQUENCY_HZ;
}

board_motor_output_init_result_t board_motor_output_initialize(
    uint16_t bit_period_ticks)
{
    const flightcomputer_v1_motor_output_group_t *group =
        flightcomputer_v1_motor_output_group();
    size_t physical_output;

    if ((bit_period_ticks == 0U) ||
        (FLIGHTCOMPUTER_V1_MOTOR_DMA_INTERRUPT_PRIORITY >=
         (UINT32_C(1) << __NVIC_PRIO_BITS)) ||
        !routes_are_valid() || (group == NULL) ||
        (group->timer_instance != 8U) ||
        (group->dma_controller != 2U) || (group->dma_stream != 1U) ||
        (group->dma_channel != 7U) ||
        (group->first_compare_register != 1U) ||
        (group->compare_register_count != 4U)) {
        return BOARD_MOTOR_OUTPUT_INIT_INVALID_ARGUMENT;
    }
    if (board_output_state != BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED) {
        return BOARD_MOTOR_OUTPUT_INIT_HARDWARE_ERROR;
    }
    if (tim8_input_clock_frequency_hz() !=
        FLIGHTCOMPUTER_V1_MOTOR_TIMER_CLOCK_FREQUENCY_HZ) {
        return BOARD_MOTOR_OUTPUT_INIT_CLOCK_ERROR;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_TIM8_CLK_ENABLE();

    drive_motor_pins_low();
    for (physical_output = 0U;
         physical_output < FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT;
         physical_output++) {
        const flightcomputer_v1_motor_output_route_t *route =
            flightcomputer_v1_motor_output_route(physical_output);
        const uint32_t pin_mask = UINT32_C(1) << route->gpio_pin;
        const uint32_t speed_shift = (uint32_t)route->gpio_pin * 2U;
        const uint32_t alternate_shift =
            ((uint32_t)route->gpio_pin % 8U) * 4U;
        const uint32_t alternate_mask = UINT32_C(15) << alternate_shift;
        volatile uint32_t *alternate_register =
            &GPIOC->AFR[route->gpio_pin / 8U];

        GPIOC->OTYPER &= ~pin_mask;
        GPIOC->OSPEEDR =
            (GPIOC->OSPEEDR & ~(UINT32_C(3) << speed_shift)) |
            (UINT32_C(3) << speed_shift);
        GPIOC->PUPDR &= ~(UINT32_C(3) << speed_shift);
        *alternate_register =
            (*alternate_register & ~alternate_mask) |
            ((uint32_t)route->gpio_alternate_function << alternate_shift);
    }

    motor_bit_period_ticks = bit_period_ticks;
    TIM8->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
    TIM8->CR2 = 0U;
    TIM8->SMCR = 0U;
    TIM8->DIER = 0U;
    TIM8->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 |
                  TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2PE |
                  TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM8->CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_1 |
                  TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4PE |
                  TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;
    TIM8->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E |
                 TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM8->PSC = 0U;
    TIM8->ARR = (uint32_t)bit_period_ticks - 1U;
    TIM8->RCR = 0U;
    TIM8->BDTR = TIM_BDTR_MOE;
    TIM8->DCR = MOTOR_TIMER_DMA_BASE_CCR1 |
                (MOTOR_TIMER_DMA_BURST_LENGTH_MINUS_ONE << TIM_DCR_DBL_Pos);
    clear_timer_compare_values();

    if (!disable_dma_stream()) {
        drive_motor_pins_low();
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
        return BOARD_MOTOR_OUTPUT_INIT_HARDWARE_ERROR;
    }
    DMA2_Stream1->CR = 0U;
    DMA2_Stream1->NDTR = 0U;
    DMA2_Stream1->PAR = (uint32_t)(uintptr_t)&TIM8->DMAR;
    DMA2_Stream1->M0AR = 0U;
    DMA2_Stream1->FCR = 0U;
    DMA2->LIFCR = MOTOR_DMA_STREAM_FLAGS;

    NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);
    NVIC_SetPriority(DMA2_Stream1_IRQn,
                     FLIGHTCOMPUTER_V1_MOTOR_DMA_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    board_output_state = BOARD_MOTOR_OUTPUT_STATUS_IDLE;
    return BOARD_MOTOR_OUTPUT_INIT_OK;
}

board_motor_output_submit_result_t board_motor_output_submit(
    const uint16_t *compare_values,
    size_t compare_value_count)
{
    uint32_t interrupt_state;

    if ((board_output_state == BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED) ||
        !compare_values_are_valid(compare_values, compare_value_count)) {
        return BOARD_MOTOR_OUTPUT_SUBMIT_ERROR;
    }

    interrupt_state = __get_PRIMASK();
    __disable_irq();
    if (board_output_state == BOARD_MOTOR_OUTPUT_STATUS_ACTIVE) {
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_SUBMIT_BUSY;
    }
    if (board_output_state != BOARD_MOTOR_OUTPUT_STATUS_IDLE) {
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_SUBMIT_ERROR;
    }

    if (!copy_physical_values_to_dma_order(compare_values,
                                           compare_value_count)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_SUBMIT_ERROR;
    }

    board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ACTIVE;
    if (!start_transfer(motor_dma_compare_values,
                        compare_value_count,
                        true)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_SUBMIT_ERROR;
    }
    if (interrupt_state == 0U) {
        __enable_irq();
    }
    return BOARD_MOTOR_OUTPUT_SUBMIT_ACCEPTED;
}

board_motor_output_stop_result_t board_motor_output_force_stop(
    const uint16_t *stop_compare_values,
    size_t compare_value_count)
{
    uint32_t interrupt_state;
    uint32_t remaining = MOTOR_STOP_TRANSFER_WAIT_LIMIT;
    uint32_t flags = 0U;
    bool transfer_started;
    bool hardware_stopped;

    if (board_output_state == BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED) {
        return BOARD_MOTOR_OUTPUT_STOP_ERROR;
    }

    interrupt_state = __get_PRIMASK();
    __disable_irq();
    hardware_stopped = stop_transfer_hardware();
    NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);

    if (!compare_values_are_valid(stop_compare_values,
                                  compare_value_count)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_STOP_ERROR;
    }
    if (!copy_physical_values_to_dma_order(stop_compare_values,
                                           compare_value_count)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        return BOARD_MOTOR_OUTPUT_STOP_ERROR;
    }

    board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ACTIVE;
    transfer_started = start_transfer(motor_dma_compare_values,
                                      compare_value_count,
                                      false);
    while (transfer_started && (remaining > 0U)) {
        flags = DMA2->LISR;
        if (((flags & MOTOR_DMA_ERROR_FLAGS) != 0U) ||
            ((flags & DMA_LISR_TCIF1) != 0U)) {
            break;
        }
        remaining--;
    }

    hardware_stopped = stop_transfer_hardware() && hardware_stopped;
    NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);
    if (interrupt_state == 0U) {
        __enable_irq();
    }

    if (transfer_started && hardware_stopped && (remaining > 0U) &&
        ((flags & MOTOR_DMA_ERROR_FLAGS) == 0U) &&
        ((flags & DMA_LISR_TCIF1) != 0U)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_IDLE;
        return BOARD_MOTOR_OUTPUT_STOP_ACCEPTED;
    }

    board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
    return BOARD_MOTOR_OUTPUT_STOP_ERROR;
}

board_motor_output_status_t board_motor_output_status(void)
{
    return board_output_state;
}

void DMA2_Stream1_IRQHandler(void)
{
    const uint32_t flags = DMA2->LISR;
    const bool transfer_error = (flags & MOTOR_DMA_ERROR_FLAGS) != 0U;
    const bool transfer_complete = (flags & DMA_LISR_TCIF1) != 0U;
    const bool hardware_stopped = stop_transfer_hardware();

    if (transfer_error || !transfer_complete || !hardware_stopped ||
        (board_output_state != BOARD_MOTOR_OUTPUT_STATUS_ACTIVE)) {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
    } else {
        board_output_state = BOARD_MOTOR_OUTPUT_STATUS_IDLE;
    }
}
