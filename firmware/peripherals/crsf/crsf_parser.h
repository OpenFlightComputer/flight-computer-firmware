#ifndef OPENFLIGHTCOMPUTER_CRSF_PARSER_H
#define OPENFLIGHTCOMPUTER_CRSF_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRSF_FRAME_CAPACITY 64U

typedef struct {
    uint8_t type;
    const uint8_t *payload;
    size_t payload_length;
} crsf_frame_t;

typedef struct {
    uint8_t buffer[CRSF_FRAME_CAPACITY];
    size_t received_length;
    size_t expected_length;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t framing_error_count;
    bool last_push_rejected_frame;
} crsf_parser_t;

void crsf_parser_initialize(crsf_parser_t *parser);
bool crsf_parser_push_byte(crsf_parser_t *parser,
                           uint8_t byte,
                           crsf_frame_t *frame);
uint8_t crsf_crc8(const uint8_t *data, size_t length);

#endif
