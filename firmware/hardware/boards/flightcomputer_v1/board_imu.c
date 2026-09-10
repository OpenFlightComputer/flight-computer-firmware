#include "board_imu.h"

#include "board_definition.h"

#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stddef.h>

#define IMU_SPI_TRANSFER_TIMEOUT_MS UINT32_C(20)

_Static_assert(
    (FLIGHTCOMPUTER_V1_APB1_PERIPHERAL_CLOCK_FREQUENCY_HZ / 64U) ==
        FLIGHTCOMPUTER_V1_IMU_SPI_FREQUENCY_HZ,
    "SPI3 prescaler must produce the tester-proven BMI270 clock");

typedef struct {
    SPI_HandleTypeDef handle;
    bool initialized;
} board_imu_context_t;

static board_imu_context_t imu_context;

static bool initialize(spi_device_t *device)
{
    board_imu_context_t *context;
    GPIO_InitTypeDef pins = {0};

    if ((device == NULL) || (device->context == NULL)) {
        return false;
    }
    context = device->context;
    if (context->initialized) {
        return true;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();

    pins.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOB, &pins);

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    pins.Pin = GPIO_PIN_2;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = 0U;
    HAL_GPIO_Init(GPIOD, &pins);

    context->handle.Instance = SPI3;
    context->handle.Init.Mode = SPI_MODE_MASTER;
    context->handle.Init.Direction = SPI_DIRECTION_2LINES;
    context->handle.Init.DataSize = SPI_DATASIZE_8BIT;
    context->handle.Init.CLKPolarity = SPI_POLARITY_LOW;
    context->handle.Init.CLKPhase = SPI_PHASE_1EDGE;
    context->handle.Init.NSS = SPI_NSS_SOFT;
    context->handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    context->handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    context->handle.Init.TIMode = SPI_TIMODE_DISABLE;
    context->handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    context->handle.Init.CRCPolynomial = 7U;
    context->initialized = HAL_SPI_Init(&context->handle) == HAL_OK;
    return context->initialized;
}

static bool select(spi_device_t *device)
{
    if (!initialize(device)) {
        return false;
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
    return true;
}

static void deselect(spi_device_t *device)
{
    (void)device;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
}

static bool transfer(spi_device_t *device,
                     const uint8_t *transmit,
                     uint8_t *receive,
                     size_t length)
{
    board_imu_context_t *context;

    if ((device == NULL) || (device->context == NULL) ||
        (transmit == NULL) || (receive == NULL) || (length == 0U) ||
        (length > UINT16_MAX) || !initialize(device)) {
        return false;
    }
    context = device->context;
    return HAL_SPI_TransmitReceive(&context->handle,
                                   (uint8_t *)transmit,
                                   receive,
                                   (uint16_t)length,
                                   IMU_SPI_TRANSFER_TIMEOUT_MS) == HAL_OK;
}

static void delay_us(spi_device_t *device, uint32_t duration_us)
{
    uint32_t cycles_per_us;
    uint32_t remaining_us = duration_us;

    (void)device;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cycles_per_us = SystemCoreClock / UINT32_C(1000000);

    /* Chunk the wait so a large vendor delay cannot overflow the cycle count. */
    while (remaining_us > 0U) {
        const uint32_t chunk_us =
            remaining_us > (UINT32_MAX / cycles_per_us)
                ? UINT32_MAX / cycles_per_us
                : remaining_us;
        const uint32_t cycle_count = chunk_us * cycles_per_us;
        const uint32_t started_at = DWT->CYCCNT;

        while ((uint32_t)(DWT->CYCCNT - started_at) < cycle_count) {
            __NOP();
        }
        remaining_us -= chunk_us;
    }
}

static const spi_device_operations_t imu_operations = {
    .initialize = initialize,
    .select = select,
    .deselect = deselect,
    .transfer = transfer,
    .delay_us = delay_us,
};

static spi_device_t imu_device = {
    .operations = &imu_operations,
    .context = &imu_context,
};

spi_device_t *board_imu_spi_device(void)
{
    return &imu_device;
}
