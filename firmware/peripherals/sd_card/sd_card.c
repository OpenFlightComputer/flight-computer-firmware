#include "sd_card.h"

#include "sd_card_csd.h"

#include <stddef.h>
#include <string.h>

#define SD_COMMAND_GO_IDLE_STATE UINT8_C(0)
#define SD_COMMAND_SEND_IF_COND UINT8_C(8)
#define SD_COMMAND_SEND_CSD UINT8_C(9)
#define SD_COMMAND_SET_BLOCKLEN UINT8_C(16)
#define SD_COMMAND_READ_SINGLE_BLOCK UINT8_C(17)
#define SD_COMMAND_WRITE_SINGLE_BLOCK UINT8_C(24)
#define SD_COMMAND_APP_COMMAND UINT8_C(55)
#define SD_COMMAND_READ_OCR UINT8_C(58)
#define SD_ACOMMAND_SEND_OP_COND UINT8_C(41)
#define SD_RESPONSE_ATTEMPTS UINT32_C(16)
#define SD_READY_TIMEOUT_US UINT64_C(1000000)
#define SD_DATA_TIMEOUT_US UINT64_C(250000)
#define SD_DATA_START_TOKEN UINT8_C(0xFE)
#define SD_DATA_ACCEPTED UINT8_C(0x05)
#define SD_INITIAL_FREQUENCY_HZ UINT32_C(328125)
#define SD_DATA_FREQUENCY_HZ UINT32_C(21000000)

static bool transfer_byte(sd_card_t *card, uint8_t sent, uint8_t *received)
{
    return spi_device_transfer(card->device, &sent, received, 1U);
}

static bool clock_bytes(sd_card_t *card, uint32_t count)
{
    uint8_t received;
    while (count-- > 0U) {
        if (!transfer_byte(card, UINT8_C(0xFF), &received)) {
            return false;
        }
    }
    return true;
}

static bool read_bytes(sd_card_t *card, uint8_t *buffer, size_t length)
{
    size_t index;
    for (index = 0U; index < length; index++) {
        if (!transfer_byte(card, UINT8_C(0xFF), &buffer[index])) {
            return false;
        }
    }
    return true;
}

static bool command(sd_card_t *card,
                    uint8_t index,
                    uint32_t argument,
                    uint8_t crc,
                    uint8_t *response)
{
    uint8_t frame[6] = {
        (uint8_t)(UINT8_C(0x40) | index),
        (uint8_t)(argument >> 24U),
        (uint8_t)(argument >> 16U),
        (uint8_t)(argument >> 8U),
        (uint8_t)argument,
        crc,
    };
    uint8_t ignored[sizeof(frame)];
    uint32_t attempt;

    if (!spi_device_transfer(card->device, frame, ignored, sizeof(frame))) {
        return false;
    }
    for (attempt = 0U; attempt < SD_RESPONSE_ATTEMPTS; attempt++) {
        if (!transfer_byte(card, UINT8_C(0xFF), response)) {
            return false;
        }
        if ((*response & UINT8_C(0x80)) == 0U) {
            return true;
        }
    }
    return false;
}

static void deselect(sd_card_t *card)
{
    uint8_t ignored;
    const uint8_t idle = UINT8_C(0xFF);
    spi_device_deselect(card->device);
    (void)spi_device_transfer(card->device, &idle, &ignored, 1U);
}

static bool select_ready_blocking(sd_card_t *card, uint64_t timeout_us)
{
    const uint64_t started_at = card->clock();
    uint8_t value;

    if (!spi_device_select(card->device)) {
        return false;
    }
    do {
        if (!transfer_byte(card, UINT8_C(0xFF), &value)) {
            return false;
        }
        if (value == UINT8_C(0xFF)) {
            return true;
        }
    } while ((card->clock() - started_at) < timeout_us);
    return false;
}

static bool select_ready_once(sd_card_t *card)
{
    uint8_t value;
    return spi_device_select(card->device) &&
           transfer_byte(card, UINT8_C(0xFF), &value) &&
           (value == UINT8_C(0xFF));
}

static bool wait_data_token(sd_card_t *card)
{
    const uint64_t started_at = card->clock();
    uint8_t token;
    do {
        if (!transfer_byte(card, UINT8_C(0xFF), &token)) {
            return false;
        }
        if (token == SD_DATA_START_TOKEN) {
            return true;
        }
    } while ((token == UINT8_C(0xFF)) &&
             ((card->clock() - started_at) < SD_DATA_TIMEOUT_US));
    return false;
}

static uint32_t wire_address(const sd_card_t *card, uint32_t sector)
{
    return card->high_capacity ? sector : sector * SD_CARD_SECTOR_SIZE;
}

static bool read_csd(sd_card_t *card)
{
    uint8_t response;
    uint8_t csd[16];
    uint8_t crc[2];

    if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
        !command(card, SD_COMMAND_SEND_CSD, 0U, UINT8_C(0xFF), &response) ||
        (response != 0U) || !wait_data_token(card) ||
        !read_bytes(card, csd, sizeof(csd)) ||
        !read_bytes(card, crc, sizeof(crc))) {
        deselect(card);
        return false;
    }
    deselect(card);
    return sd_card_parse_csd(csd, &card->sector_count);
}

sd_card_result_t sd_card_initialize(sd_card_t *card,
                                    spi_device_t *device,
                                    sd_card_clock_t clock,
                                    bool media_present)
{
    uint8_t response = UINT8_C(0xFF);
    uint8_t r7[4];
    uint8_t ocr[4];
    bool version_two;
    const uint64_t started_at = clock != NULL ? clock() : 0U;

    if ((card == NULL) || (device == NULL) || (clock == NULL)) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    *card = (sd_card_t){.device = device, .clock = clock};
    if (!media_present) {
        card->last_result = SD_CARD_RESULT_NO_MEDIA;
        return card->last_result;
    }
    if (!spi_device_set_frequency_hz(device, SD_INITIAL_FREQUENCY_HZ) ||
        !spi_device_initialize(device) || !clock_bytes(card, 10U)) {
        card->last_result = SD_CARD_RESULT_TRANSPORT_ERROR;
        return card->last_result;
    }
    if (!spi_device_select(device) ||
        !command(card, SD_COMMAND_GO_IDLE_STATE, 0U, UINT8_C(0x95),
                 &response) ||
        (response != 1U)) {
        deselect(card);
        card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
        return card->last_result;
    }
    deselect(card);
    if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
        !command(card, SD_COMMAND_SEND_IF_COND, UINT32_C(0x1AA),
                 UINT8_C(0x87), &response)) {
        deselect(card);
        card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
        return card->last_result;
    }
    if (response == 1U) {
        version_two = read_bytes(card, r7, sizeof(r7)) &&
                      (r7[2] == 1U) && (r7[3] == UINT8_C(0xAA));
        if (!version_two) {
            deselect(card);
            card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
            return card->last_result;
        }
    } else if (response == 5U) {
        version_two = false;
    } else {
        deselect(card);
        card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
        return card->last_result;
    }
    deselect(card);

    do {
        uint8_t prefix;
        if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
            !command(card, SD_COMMAND_APP_COMMAND, 0U, UINT8_C(0xFF),
                     &prefix) ||
            ((prefix != 0U) && (prefix != 1U)) ||
            !command(card, SD_ACOMMAND_SEND_OP_COND,
                     version_two ? UINT32_C(0x40000000) : 0U,
                     UINT8_C(0xFF), &response)) {
            deselect(card);
            card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
            return card->last_result;
        }
        deselect(card);
        if (response == 0U) {
            break;
        }
    } while ((clock() - started_at) < SD_READY_TIMEOUT_US);
    if (response != 0U) {
        card->last_result = SD_CARD_RESULT_TIMEOUT;
        return card->last_result;
    }

    if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
        !command(card, SD_COMMAND_READ_OCR, 0U, UINT8_C(0xFF), &response) ||
        (response != 0U) || !read_bytes(card, ocr, sizeof(ocr))) {
        deselect(card);
        card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
        return card->last_result;
    }
    card->high_capacity = version_two && ((ocr[0] & UINT8_C(0x40)) != 0U);
    deselect(card);
    if (!card->high_capacity) {
        if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
            !command(card, SD_COMMAND_SET_BLOCKLEN, SD_CARD_SECTOR_SIZE,
                     UINT8_C(0xFF), &response) ||
            (response != 0U)) {
            deselect(card);
            card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
            return card->last_result;
        }
        deselect(card);
    }
    if (!read_csd(card) || (card->sector_count <= 2U) ||
        !spi_device_set_frequency_hz(device, SD_DATA_FREQUENCY_HZ)) {
        card->last_result = SD_CARD_RESULT_PROTOCOL_ERROR;
        return card->last_result;
    }
    card->write_state = SD_CARD_WRITE_IDLE;
    card->initialized = true;
    card->last_result = SD_CARD_RESULT_OK;
    return card->last_result;
}

sd_card_result_t sd_card_read_sector(sd_card_t *card,
                                     uint32_t sector,
                                     uint8_t destination[SD_CARD_SECTOR_SIZE])
{
    uint8_t response;
    uint8_t crc[2];

    if ((card == NULL) || (destination == NULL)) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    if (!card->initialized) {
        return SD_CARD_RESULT_NOT_INITIALIZED;
    }
    if ((uint64_t)sector >= card->sector_count) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    if (card->write_state != SD_CARD_WRITE_IDLE) {
        return SD_CARD_RESULT_BUSY;
    }
    if (!select_ready_blocking(card, SD_READY_TIMEOUT_US) ||
        !command(card, SD_COMMAND_READ_SINGLE_BLOCK,
                 wire_address(card, sector), UINT8_C(0xFF), &response) ||
        (response != 0U) || !wait_data_token(card) ||
        !read_bytes(card, destination, SD_CARD_SECTOR_SIZE) ||
        !read_bytes(card, crc, sizeof(crc))) {
        deselect(card);
        return SD_CARD_RESULT_PROTOCOL_ERROR;
    }
    deselect(card);
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_start(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE])
{
    uint8_t response;
    spi_device_async_result_t transfer_result;

    if ((card == NULL) || (source == NULL)) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    if (!card->initialized) {
        return SD_CARD_RESULT_NOT_INITIALIZED;
    }
    if ((uint64_t)sector >= card->sector_count) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    if (card->write_state != SD_CARD_WRITE_IDLE) {
        return SD_CARD_RESULT_BUSY;
    }
    if (!select_ready_once(card)) {
        deselect(card);
        return SD_CARD_RESULT_BUSY;
    }
    if (!command(card, SD_COMMAND_WRITE_SINGLE_BLOCK,
                 wire_address(card, sector), UINT8_C(0xFF), &response) ||
        (response != 0U)) {
        deselect(card);
        return SD_CARD_RESULT_PROTOCOL_ERROR;
    }
    card->transmit_buffer[0] = SD_DATA_START_TOKEN;
    memcpy(&card->transmit_buffer[1], source, SD_CARD_SECTOR_SIZE);
    card->transmit_buffer[SD_CARD_SECTOR_SIZE + 1U] = UINT8_C(0xFF);
    card->transmit_buffer[SD_CARD_SECTOR_SIZE + 2U] = UINT8_C(0xFF);
    transfer_result = spi_device_transfer_start(
        card->device, card->transmit_buffer, card->receive_buffer,
        sizeof(card->transmit_buffer));
    if (transfer_result != SPI_DEVICE_ASYNC_STARTED) {
        deselect(card);
        return transfer_result == SPI_DEVICE_ASYNC_BUSY
                   ? SD_CARD_RESULT_BUSY
                   : SD_CARD_RESULT_TRANSPORT_ERROR;
    }
    card->active_sector = sector;
    card->write_started_at_us = card->clock();
    card->write_state = SD_CARD_WRITE_DATA_TRANSFER;
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_poll(sd_card_t *card)
{
    uint8_t response;

    if (card == NULL) {
        return SD_CARD_RESULT_INVALID_ARGUMENT;
    }
    if (!card->initialized) {
        return SD_CARD_RESULT_NOT_INITIALIZED;
    }
    if (card->write_state == SD_CARD_WRITE_IDLE) {
        return SD_CARD_RESULT_OK;
    }
    if ((card->clock() - card->write_started_at_us) >= SD_READY_TIMEOUT_US) {
        deselect(card);
        card->write_state = SD_CARD_WRITE_IDLE;
        return SD_CARD_RESULT_TIMEOUT;
    }
    if (card->write_state == SD_CARD_WRITE_DATA_TRANSFER) {
        const spi_device_async_result_t result =
            spi_device_transfer_poll(card->device);
        if (result == SPI_DEVICE_ASYNC_BUSY) {
            return SD_CARD_RESULT_BUSY;
        }
        if (result != SPI_DEVICE_ASYNC_COMPLETE) {
            deselect(card);
            card->write_state = SD_CARD_WRITE_IDLE;
            return SD_CARD_RESULT_TRANSPORT_ERROR;
        }
        card->write_state = SD_CARD_WRITE_DATA_RESPONSE;
    }
    if (card->write_state == SD_CARD_WRITE_DATA_RESPONSE) {
        if (!transfer_byte(card, UINT8_C(0xFF), &response)) {
            deselect(card);
            card->write_state = SD_CARD_WRITE_IDLE;
            return SD_CARD_RESULT_TRANSPORT_ERROR;
        }
        if ((response & UINT8_C(0x1F)) != SD_DATA_ACCEPTED) {
            deselect(card);
            card->write_state = SD_CARD_WRITE_IDLE;
            return SD_CARD_RESULT_PROTOCOL_ERROR;
        }
        card->write_state = SD_CARD_WRITE_PROGRAMMING;
    }
    if (!transfer_byte(card, UINT8_C(0xFF), &response)) {
        deselect(card);
        card->write_state = SD_CARD_WRITE_IDLE;
        return SD_CARD_RESULT_TRANSPORT_ERROR;
    }
    if (response != UINT8_C(0xFF)) {
        return SD_CARD_RESULT_BUSY;
    }
    deselect(card);
    card->write_state = SD_CARD_WRITE_IDLE;
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_blocking(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE])
{
    sd_card_result_t result = sd_card_write_sector_start(card, sector, source);
    if (result != SD_CARD_RESULT_OK) {
        return result;
    }
    do {
        result = sd_card_write_sector_poll(card);
    } while (result == SD_CARD_RESULT_BUSY);
    return result;
}
