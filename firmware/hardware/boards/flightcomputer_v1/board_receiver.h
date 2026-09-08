#ifndef OPENFLIGHTCOMPUTER_BOARD_RECEIVER_H
#define OPENFLIGHTCOMPUTER_BOARD_RECEIVER_H

#include "receiver_source.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOARD_RECEIVER_INIT_OK = 0,
    BOARD_RECEIVER_INIT_INVALID_ARGUMENT,
    BOARD_RECEIVER_INIT_DMA_ERROR,
    BOARD_RECEIVER_INIT_UART_ERROR,
    BOARD_RECEIVER_INIT_RECEIVE_ERROR,
    BOARD_RECEIVER_INIT_SOURCE_ERROR,
} board_receiver_init_result_t;

typedef struct {
    uint32_t uart_received_byte_count;
    uint32_t uart_error;
    uint32_t valid_frame_count;
    uint32_t channel_frame_count;
    uint32_t link_statistics_frame_count;
    uint32_t crc_error_count;
    uint32_t framing_error_count;
    uint32_t dma_overrun_count;
    uint32_t dma_dropped_byte_count;
    int16_t uplink_rssi_dbm;
    uint8_t uplink_link_quality_percent;
    int8_t uplink_snr_db;
    bool link_statistics_present;
} board_receiver_statistics_t;

board_receiver_init_result_t board_receiver_initialize(
    receiver_source_t *source);
bool board_receiver_statistics(board_receiver_statistics_t *statistics);

#endif
