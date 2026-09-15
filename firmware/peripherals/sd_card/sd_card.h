#ifndef OPENFLIGHTCOMPUTER_SD_CARD_H
#define OPENFLIGHTCOMPUTER_SD_CARD_H

#include "spi_device.h"

#include <stdbool.h>
#include <stdint.h>

#define SD_CARD_SECTOR_SIZE 512U

typedef uint64_t (*sd_card_clock_t)(void);

typedef enum {
    SD_CARD_RESULT_OK = 0,
    SD_CARD_RESULT_BUSY,
    SD_CARD_RESULT_NOT_INITIALIZED,
    SD_CARD_RESULT_INVALID_ARGUMENT,
    SD_CARD_RESULT_TRANSPORT_ERROR,
    SD_CARD_RESULT_PROTOCOL_ERROR,
    SD_CARD_RESULT_TIMEOUT,
    SD_CARD_RESULT_NO_MEDIA,
} sd_card_result_t;

typedef enum {
    SD_CARD_WRITE_IDLE = 0,
    SD_CARD_WRITE_DATA_TRANSFER,
    SD_CARD_WRITE_DATA_RESPONSE,
    SD_CARD_WRITE_PROGRAMMING,
} sd_card_write_state_t;

typedef struct {
    spi_device_t *device;
    sd_card_clock_t clock;
    uint64_t sector_count;
    uint64_t write_started_at_us;
    uint32_t active_sector;
    sd_card_result_t last_result;
    sd_card_write_state_t write_state;
    bool high_capacity;
    bool initialized;
    uint8_t transmit_buffer[SD_CARD_SECTOR_SIZE + 3U];
    uint8_t receive_buffer[SD_CARD_SECTOR_SIZE + 3U];
} sd_card_t;

sd_card_result_t sd_card_initialize(sd_card_t *card,
                                    spi_device_t *device,
                                    sd_card_clock_t clock,
                                    bool media_present);
sd_card_result_t sd_card_read_sector(sd_card_t *card,
                                     uint32_t sector,
                                     uint8_t destination[SD_CARD_SECTOR_SIZE]);
sd_card_result_t sd_card_write_sector_start(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE]);
sd_card_result_t sd_card_write_sector_poll(sd_card_t *card);
sd_card_result_t sd_card_write_sector_blocking(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE]);

#endif
