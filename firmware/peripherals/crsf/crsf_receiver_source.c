#include "crsf_receiver_source.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

_Static_assert(CRSF_CHANNEL_COUNT == RECEIVER_CHANNEL_COUNT,
               "CRSF and receiver channel counts must match");

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

bool crsf_receiver_source_initialize(crsf_receiver_source_t *source,
                                     const crsf_byte_stream_t *stream)
{
    if (source == NULL) {
        return false;
    }

    *source = (crsf_receiver_source_t){0};
    if ((stream == NULL) || (stream->read == NULL) ||
        (stream->error == NULL)) {
        return false;
    }

    source->stream = *stream;
    crsf_parser_initialize(&source->parser);
    source->initialized = true;
    return true;
}

receiver_source_t crsf_receiver_source_interface(
    crsf_receiver_source_t *source)
{
    return (receiver_source_t){
        .read = crsf_receiver_source_read,
        .context = source,
    };
}

receiver_source_result_t crsf_receiver_source_read(
    void *context,
    receiver_channel_frame_t *frame)
{
    crsf_receiver_source_t *source = context;
    bool invalid_frame_seen = false;
    size_t byte_count;

    if ((source == NULL) || !source->initialized || (frame == NULL) ||
        (source->stream.read == NULL) || (source->stream.error == NULL)) {
        return RECEIVER_SOURCE_ERROR;
    }
    if (source->stream.error(source->stream.context) != 0U) {
        return RECEIVER_SOURCE_ERROR;
    }

    for (byte_count = 0U;
         byte_count < CRSF_RECEIVER_SOURCE_BYTE_BUDGET;
         byte_count++) {
        crsf_channels_t channels;
        crsf_frame_t parsed_frame;
        uint8_t byte;

        if (!source->stream.read(source->stream.context, &byte)) {
            break;
        }
        saturating_increment(&source->processed_byte_count);
        if (!crsf_parser_push_byte(&source->parser, byte, &parsed_frame)) {
            invalid_frame_seen = invalid_frame_seen ||
                                 source->parser.last_push_rejected_frame;
            continue;
        }
        if (crsf_decode_channels(&parsed_frame, &channels)) {
            memcpy(frame->channels,
                   channels.values,
                   sizeof(frame->channels));
            saturating_increment(&source->channel_frame_count);
            return RECEIVER_SOURCE_FRAME_AVAILABLE;
        }
        if (crsf_decode_link_statistics(&parsed_frame,
                                        &source->link_statistics)) {
            source->link_statistics_valid = true;
            saturating_increment(&source->link_statistics_frame_count);
        }
    }

    if (source->stream.error(source->stream.context) != 0U) {
        return RECEIVER_SOURCE_ERROR;
    }
    if (invalid_frame_seen) {
        return RECEIVER_SOURCE_INVALID_FRAME;
    }
    return RECEIVER_SOURCE_NO_FRAME;
}
