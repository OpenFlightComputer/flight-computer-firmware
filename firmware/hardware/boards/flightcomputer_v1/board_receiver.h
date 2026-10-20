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

typedef receiver_source_statistics_t board_receiver_statistics_t;

board_receiver_init_result_t board_receiver_initialize(
    receiver_source_t *source);
void board_receiver_maintain(void);
bool board_receiver_statistics(board_receiver_statistics_t *statistics);

#endif
