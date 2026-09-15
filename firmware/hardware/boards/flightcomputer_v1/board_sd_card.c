#include "board_sd_card.h"

#include "board_definition.h"

#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stddef.h>

#define SD_SPI_INITIAL_FREQUENCY_HZ UINT32_C(328125)
#define SD_SPI_DATA_FREQUENCY_HZ UINT32_C(21000000)
#define SD_SPI_TRANSFER_TIMEOUT_MS UINT32_C(20)
#define SD_DMA_INTERRUPT_PRIORITY UINT32_C(12)

_Static_assert(
    (FLIGHTCOMPUTER_V1_APB2_PERIPHERAL_CLOCK_FREQUENCY_HZ / 256U) ==
        SD_SPI_INITIAL_FREQUENCY_HZ,
    "SPI1 initial prescaler must reproduce the tester-proven SD clock");
_Static_assert(
    (FLIGHTCOMPUTER_V1_APB2_PERIPHERAL_CLOCK_FREQUENCY_HZ / 4U) ==
        SD_SPI_DATA_FREQUENCY_HZ,
    "SPI1 data prescaler must reproduce the tester-proven SD clock");

typedef struct {
    SPI_HandleTypeDef spi;
    DMA_HandleTypeDef receive_dma;
    DMA_HandleTypeDef transmit_dma;
    volatile bool transfer_complete;
    volatile bool transfer_error;
    bool transfer_active;
    bool card_detect_initialized;
    bool initialized;
    uint32_t frequency_hz;
} board_sd_card_context_t;

static board_sd_card_context_t sd_context = {
    .frequency_hz = SD_SPI_INITIAL_FREQUENCY_HZ,
};

static uint32_t prescaler_for_frequency(uint32_t frequency_hz)
{
    if (frequency_hz == SD_SPI_INITIAL_FREQUENCY_HZ) {
        return SPI_BAUDRATEPRESCALER_256;
    }
    if (frequency_hz == SD_SPI_DATA_FREQUENCY_HZ) {
        return SPI_BAUDRATEPRESCALER_4;
    }
    return 0U;
}

static bool initialize(spi_device_t *device)
{
    board_sd_card_context_t *context;
    GPIO_InitTypeDef pins = {0};
    uint32_t prescaler;

    if ((device == NULL) || (device->context == NULL)) {
        return false;
    }
    context = device->context;
    if (context->initialized) {
        return true;
    }
    prescaler = prescaler_for_frequency(context->frequency_hz);
    if (prescaler == 0U) {
        return false;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    pins.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &pins);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    pins.Pin = GPIO_PIN_4;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Alternate = 0U;
    HAL_GPIO_Init(GPIOC, &pins);

    context->spi.Instance = SPI1;
    context->spi.Init.Mode = SPI_MODE_MASTER;
    context->spi.Init.Direction = SPI_DIRECTION_2LINES;
    context->spi.Init.DataSize = SPI_DATASIZE_8BIT;
    context->spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    context->spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    context->spi.Init.NSS = SPI_NSS_SOFT;
    context->spi.Init.BaudRatePrescaler = prescaler;
    context->spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    context->spi.Init.TIMode = SPI_TIMODE_DISABLE;
    context->spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    context->spi.Init.CRCPolynomial = 7U;
    if (HAL_SPI_Init(&context->spi) != HAL_OK) {
        return false;
    }

    context->receive_dma.Instance = DMA2_Stream0;
    context->receive_dma.Init.Channel = DMA_CHANNEL_3;
    context->receive_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    context->receive_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    context->receive_dma.Init.MemInc = DMA_MINC_ENABLE;
    context->receive_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    context->receive_dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    context->receive_dma.Init.Mode = DMA_NORMAL;
    context->receive_dma.Init.Priority = DMA_PRIORITY_LOW;
    context->receive_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&context->receive_dma) != HAL_OK) {
        (void)HAL_SPI_DeInit(&context->spi);
        return false;
    }

    context->transmit_dma.Instance = DMA2_Stream3;
    context->transmit_dma.Init.Channel = DMA_CHANNEL_3;
    context->transmit_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    context->transmit_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    context->transmit_dma.Init.MemInc = DMA_MINC_ENABLE;
    context->transmit_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    context->transmit_dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    context->transmit_dma.Init.Mode = DMA_NORMAL;
    context->transmit_dma.Init.Priority = DMA_PRIORITY_LOW;
    context->transmit_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&context->transmit_dma) != HAL_OK) {
        (void)HAL_DMA_DeInit(&context->receive_dma);
        (void)HAL_SPI_DeInit(&context->spi);
        return false;
    }

    __HAL_LINKDMA(&context->spi, hdmarx, context->receive_dma);
    __HAL_LINKDMA(&context->spi, hdmatx, context->transmit_dma);
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, SD_DMA_INTERRUPT_PRIORITY, 0U);
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, SD_DMA_INTERRUPT_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    context->initialized = true;
    return true;
}

static bool select(spi_device_t *device)
{
    if (!initialize(device)) {
        return false;
    }
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
    return true;
}

static void deselect(spi_device_t *device)
{
    (void)device;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
}

static bool transfer(spi_device_t *device,
                     const uint8_t *transmit,
                     uint8_t *receive,
                     size_t length)
{
    board_sd_card_context_t *context;

    if ((device == NULL) || (device->context == NULL) ||
        (transmit == NULL) || (receive == NULL) || (length == 0U) ||
        (length > UINT16_MAX) || !initialize(device)) {
        return false;
    }
    context = device->context;
    if (context->transfer_active) {
        return false;
    }
    return HAL_SPI_TransmitReceive(&context->spi,
                                   (uint8_t *)transmit,
                                   receive,
                                   (uint16_t)length,
                                   SD_SPI_TRANSFER_TIMEOUT_MS) == HAL_OK;
}

static spi_device_async_result_t transfer_start(spi_device_t *device,
                                                const uint8_t *transmit,
                                                uint8_t *receive,
                                                size_t length)
{
    board_sd_card_context_t *context;
    HAL_StatusTypeDef result;

    if ((device == NULL) || (device->context == NULL) ||
        (transmit == NULL) || (receive == NULL) || (length == 0U) ||
        (length > UINT16_MAX) || !initialize(device)) {
        return SPI_DEVICE_ASYNC_ERROR;
    }
    context = device->context;
    if (context->transfer_active) {
        return SPI_DEVICE_ASYNC_BUSY;
    }
    context->transfer_complete = false;
    context->transfer_error = false;
    result = HAL_SPI_TransmitReceive_DMA(&context->spi,
                                         (uint8_t *)transmit,
                                         receive,
                                         (uint16_t)length);
    if (result == HAL_BUSY) {
        return SPI_DEVICE_ASYNC_BUSY;
    }
    if (result != HAL_OK) {
        return SPI_DEVICE_ASYNC_ERROR;
    }
    context->transfer_active = true;
    return SPI_DEVICE_ASYNC_STARTED;
}

static spi_device_async_result_t transfer_poll(spi_device_t *device)
{
    board_sd_card_context_t *context;

    if ((device == NULL) || (device->context == NULL)) {
        return SPI_DEVICE_ASYNC_ERROR;
    }
    context = device->context;
    if (!context->transfer_active) {
        return SPI_DEVICE_ASYNC_COMPLETE;
    }
    if (context->transfer_error) {
        context->transfer_active = false;
        return SPI_DEVICE_ASYNC_ERROR;
    }
    if (!context->transfer_complete) {
        return SPI_DEVICE_ASYNC_BUSY;
    }
    context->transfer_active = false;
    return SPI_DEVICE_ASYNC_COMPLETE;
}

static bool set_frequency_hz(spi_device_t *device, uint32_t frequency_hz)
{
    board_sd_card_context_t *context;

    if ((device == NULL) || (device->context == NULL) ||
        (prescaler_for_frequency(frequency_hz) == 0U)) {
        return false;
    }
    context = device->context;
    if (context->transfer_active) {
        return false;
    }
    if (context->initialized) {
        HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
        HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
        (void)HAL_DMA_DeInit(&context->receive_dma);
        (void)HAL_DMA_DeInit(&context->transmit_dma);
        if (HAL_SPI_DeInit(&context->spi) != HAL_OK) {
            return false;
        }
        context->initialized = false;
    }
    context->frequency_hz = frequency_hz;
    return initialize(device);
}

static void delay_us(spi_device_t *device, uint32_t duration_us)
{
    uint32_t cycles_per_us;
    uint32_t started_at;
    uint64_t requested_cycles;
    (void)device;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    started_at = DWT->CYCCNT;
    cycles_per_us = SystemCoreClock / UINT32_C(1000000);
    requested_cycles = (uint64_t)duration_us * cycles_per_us;
    if (requested_cycles > UINT32_MAX) {
        requested_cycles = UINT32_MAX;
    }
    while ((uint64_t)(uint32_t)(DWT->CYCCNT - started_at) <
           requested_cycles) {
        __NOP();
    }
}

static const spi_device_operations_t sd_operations = {
    .initialize = initialize,
    .select = select,
    .deselect = deselect,
    .transfer = transfer,
    .transfer_start = transfer_start,
    .transfer_poll = transfer_poll,
    .set_frequency_hz = set_frequency_hz,
    .delay_us = delay_us,
};

static spi_device_t sd_device = {
    .operations = &sd_operations,
    .context = &sd_context,
};

spi_device_t *board_sd_card_spi_device(void)
{
    return &sd_device;
}

bool board_sd_card_inserted(void)
{
    GPIO_InitTypeDef pin = {0};

    if (!sd_context.card_detect_initialized) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        pin.Pin = GPIO_PIN_5;
        pin.Mode = GPIO_MODE_INPUT;
        pin.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &pin);
        sd_context.card_detect_initialized = true;
    }
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_RESET;
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&sd_context.receive_dma);
}

void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&sd_context.transmit_dma);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi)
{
    if (spi == &sd_context.spi) {
        sd_context.transfer_complete = true;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi)
{
    if (spi == &sd_context.spi) {
        sd_context.transfer_error = true;
    }
}
