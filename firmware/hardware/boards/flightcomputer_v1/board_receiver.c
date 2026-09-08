#include "board_receiver.h"

#include "board_definition.h"
#include "crsf_receiver_source.h"

#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stddef.h>

static UART_HandleTypeDef receiver_uart;
static DMA_HandleTypeDef receiver_dma;
static uint8_t receiver_dma_buffer[FLIGHTCOMPUTER_V1_RECEIVER_DMA_CAPACITY];
static crsf_receiver_source_t receiver_crsf_source;
static size_t receiver_read_position;
static uint32_t receiver_received_byte_count;
static volatile uint32_t receiver_last_error;
static bool receiver_dma_initialized;
static bool receiver_uart_initialized;
static bool receiver_initialized;

static size_t receiver_dma_write_position(void)
{
    return FLIGHTCOMPUTER_V1_RECEIVER_DMA_CAPACITY -
           (size_t)__HAL_DMA_GET_COUNTER(&receiver_dma);
}

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

static bool receiver_read_byte(void *context, uint8_t *byte)
{
    size_t write_position;

    (void)context;
    if (!receiver_initialized || (byte == NULL)) {
        return false;
    }

    write_position = receiver_dma_write_position();
    if (write_position == receiver_read_position) {
        return false;
    }

    *byte = receiver_dma_buffer[receiver_read_position];
    receiver_read_position = (receiver_read_position + 1U) %
                             FLIGHTCOMPUTER_V1_RECEIVER_DMA_CAPACITY;
    saturating_increment(&receiver_received_byte_count);
    return true;
}

static uint32_t receiver_error(void *context)
{
    (void)context;
    return receiver_last_error;
}

static void receiver_release_hardware(void)
{
    HAL_NVIC_DisableIRQ(UART4_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream2_IRQn);

    if (receiver_uart_initialized) {
        (void)HAL_UART_DMAStop(&receiver_uart);
        (void)HAL_UART_DeInit(&receiver_uart);
        receiver_uart_initialized = false;
    }
    if (receiver_dma_initialized) {
        (void)HAL_DMA_DeInit(&receiver_dma);
        receiver_dma_initialized = false;
    }

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11);
    __HAL_RCC_UART4_CLK_DISABLE();
    receiver_initialized = false;
}

board_receiver_init_result_t board_receiver_initialize(
    receiver_source_t *source)
{
    const crsf_byte_stream_t byte_stream = {
        .read = receiver_read_byte,
        .error = receiver_error,
    };
    GPIO_InitTypeDef pins = {0};

    if (source == NULL) {
        return BOARD_RECEIVER_INIT_INVALID_ARGUMENT;
    }
    *source = (receiver_source_t){0};

    receiver_read_position = 0U;
    receiver_received_byte_count = 0U;
    receiver_last_error = HAL_UART_ERROR_NONE;
    receiver_dma_initialized = false;
    receiver_uart_initialized = false;
    receiver_initialized = false;

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();

    pins.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOC, &pins);

    receiver_dma.Instance = DMA1_Stream2;
    receiver_dma.Init.Channel = DMA_CHANNEL_4;
    receiver_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    receiver_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    receiver_dma.Init.MemInc = DMA_MINC_ENABLE;
    receiver_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    receiver_dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    receiver_dma.Init.Mode = DMA_CIRCULAR;
    receiver_dma.Init.Priority = DMA_PRIORITY_HIGH;
    receiver_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&receiver_dma) != HAL_OK) {
        receiver_last_error = 1U;
        receiver_release_hardware();
        return BOARD_RECEIVER_INIT_DMA_ERROR;
    }
    receiver_dma_initialized = true;

    receiver_uart.Instance = UART4;
    receiver_uart.Init.BaudRate = FLIGHTCOMPUTER_V1_RECEIVER_UART_BAUD_RATE;
    receiver_uart.Init.WordLength = UART_WORDLENGTH_8B;
    receiver_uart.Init.StopBits = UART_STOPBITS_1;
    receiver_uart.Init.Parity = UART_PARITY_NONE;
    receiver_uart.Init.Mode = UART_MODE_TX_RX;
    receiver_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    receiver_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    __HAL_LINKDMA(&receiver_uart, hdmarx, receiver_dma);
    if (HAL_UART_Init(&receiver_uart) != HAL_OK) {
        receiver_last_error = 2U;
        receiver_release_hardware();
        return BOARD_RECEIVER_INIT_UART_ERROR;
    }
    receiver_uart_initialized = true;

    HAL_NVIC_SetPriority(DMA1_Stream2_IRQn,
                         FLIGHTCOMPUTER_V1_RECEIVER_INTERRUPT_PRIORITY,
                         0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_SetPriority(UART4_IRQn,
                         FLIGHTCOMPUTER_V1_RECEIVER_INTERRUPT_PRIORITY,
                         0U);
    HAL_NVIC_EnableIRQ(UART4_IRQn);

    if (HAL_UART_Receive_DMA(&receiver_uart,
                             receiver_dma_buffer,
                             FLIGHTCOMPUTER_V1_RECEIVER_DMA_CAPACITY) !=
        HAL_OK) {
        receiver_last_error = 3U;
        receiver_release_hardware();
        return BOARD_RECEIVER_INIT_RECEIVE_ERROR;
    }
    __HAL_DMA_DISABLE_IT(&receiver_dma, DMA_IT_HT | DMA_IT_TC);
    receiver_initialized = true;

    if (!crsf_receiver_source_initialize(&receiver_crsf_source,
                                         &byte_stream)) {
        receiver_last_error = 4U;
        receiver_release_hardware();
        return BOARD_RECEIVER_INIT_SOURCE_ERROR;
    }

    *source = crsf_receiver_source_interface(&receiver_crsf_source);
    return BOARD_RECEIVER_INIT_OK;
}

bool board_receiver_statistics(board_receiver_statistics_t *statistics)
{
    if (!receiver_initialized || (statistics == NULL)) {
        return false;
    }

    *statistics = (board_receiver_statistics_t){
        .uart_received_byte_count = receiver_received_byte_count,
        .uart_error = receiver_last_error,
        .valid_frame_count = receiver_crsf_source.parser.valid_frame_count,
        .channel_frame_count = receiver_crsf_source.channel_frame_count,
        .link_statistics_frame_count =
            receiver_crsf_source.link_statistics_frame_count,
        .crc_error_count = receiver_crsf_source.parser.crc_error_count,
        .framing_error_count =
            receiver_crsf_source.parser.framing_error_count,
    };
    return true;
}

void UART4_IRQHandler(void)
{
    if (receiver_uart_initialized) {
        HAL_UART_IRQHandler(&receiver_uart);
    }
}

void DMA1_Stream2_IRQHandler(void)
{
    if (receiver_dma_initialized) {
        HAL_DMA_IRQHandler(&receiver_dma);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
    if (handle == &receiver_uart) {
        receiver_last_error = handle->ErrorCode;
    }
}
