#ifndef OPENFLIGHTCOMPUTER_CRSF_RECEIVER_SOURCE_H
#define OPENFLIGHTCOMPUTER_CRSF_RECEIVER_SOURCE_H

#include "crsf_decoder.h"
#include "crsf_parser.h"
#include "receiver_source.h"

#include <stdbool.h>
#include <stdint.h>

#define CRSF_RECEIVER_SOURCE_BYTE_BUDGET 512U

typedef bool (*crsf_byte_stream_read_t)(void *context, uint8_t *byte);
typedef uint32_t (*crsf_byte_stream_error_t)(void *context);

typedef struct {
    crsf_byte_stream_read_t read;
    crsf_byte_stream_error_t error;
    void *context;
} crsf_byte_stream_t;

typedef struct {
    crsf_byte_stream_t stream;
    crsf_parser_t parser;
    crsf_link_statistics_t link_statistics;
    uint32_t processed_byte_count;
    uint32_t channel_frame_count;
    uint32_t link_statistics_frame_count;
    bool link_statistics_valid;
    bool initialized;
} crsf_receiver_source_t;

bool crsf_receiver_source_initialize(crsf_receiver_source_t *source,
                                     const crsf_byte_stream_t *stream);
receiver_source_t crsf_receiver_source_interface(
    crsf_receiver_source_t *source);
receiver_source_result_t crsf_receiver_source_read(
    void *context,
    receiver_channel_frame_t *frame);

#endif
