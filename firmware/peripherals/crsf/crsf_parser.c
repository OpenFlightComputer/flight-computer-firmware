#include "crsf_parser.h"

#include <limits.h>
#include <stddef.h>

#define CRSF_CRC_POLYNOMIAL 0xD5U
#define CRSF_MINIMUM_LENGTH_FIELD 2U
#define CRSF_MAXIMUM_LENGTH_FIELD 62U

static bool is_frame_address(uint8_t byte)
{
    return (byte == 0x00U) || (byte == 0xC8U) || (byte == 0xEAU) ||
           (byte == 0xECU) || (byte == 0xEEU);
}

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

uint8_t crsf_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;
    size_t index;

    if (data == NULL) {
        return 0U;
    }

    for (index = 0U; index < length; index++) {
        uint8_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x80U) != 0U
                      ? (uint8_t)((crc << 1U) ^ CRSF_CRC_POLYNOMIAL)
                      : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

void crsf_parser_initialize(crsf_parser_t *parser)
{
    if (parser != NULL) {
        *parser = (crsf_parser_t){0};
    }
}

bool crsf_parser_push_byte(crsf_parser_t *parser,
                           uint8_t byte,
                           crsf_frame_t *frame)
{
    size_t crc_index;
    uint8_t expected_crc;
    bool valid;

    if ((parser == NULL) || (frame == NULL)) {
        return false;
    }
    parser->last_push_rejected_frame = false;

    if (parser->received_length == 0U) {
        if (is_frame_address(byte)) {
            parser->buffer[0] = byte;
            parser->received_length = 1U;
        }
        return false;
    }

    if (parser->received_length == 1U) {
        if ((byte < CRSF_MINIMUM_LENGTH_FIELD) ||
            (byte > CRSF_MAXIMUM_LENGTH_FIELD)) {
            saturating_increment(&parser->framing_error_count);
            parser->last_push_rejected_frame = true;
            parser->received_length = 0U;
            parser->expected_length = 0U;
            if (is_frame_address(byte)) {
                parser->buffer[0] = byte;
                parser->received_length = 1U;
            }
            return false;
        }
        parser->buffer[1] = byte;
        parser->received_length = 2U;
        parser->expected_length = (size_t)byte + 2U;
        return false;
    }

    parser->buffer[parser->received_length++] = byte;
    if (parser->received_length < parser->expected_length) {
        return false;
    }

    crc_index = parser->expected_length - 1U;
    expected_crc = crsf_crc8(&parser->buffer[2],
                             parser->expected_length - 3U);
    valid = expected_crc == parser->buffer[crc_index];

    if (valid) {
        frame->type = parser->buffer[2];
        frame->payload = &parser->buffer[3];
        frame->payload_length = parser->expected_length - 4U;
        saturating_increment(&parser->valid_frame_count);
    } else {
        saturating_increment(&parser->crc_error_count);
        parser->last_push_rejected_frame = true;
    }
    parser->received_length = 0U;
    parser->expected_length = 0U;
    return valid;
}
