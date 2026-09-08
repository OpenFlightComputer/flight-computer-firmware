#ifndef OPENFLIGHTCOMPUTER_RECEIVER_SOURCE_H
#define OPENFLIGHTCOMPUTER_RECEIVER_SOURCE_H

#include <stdint.h>

#define RECEIVER_CHANNEL_COUNT 16U

typedef struct {
    uint16_t channels[RECEIVER_CHANNEL_COUNT];
} receiver_channel_frame_t;

typedef enum {
    RECEIVER_SOURCE_NO_FRAME = 0,
    RECEIVER_SOURCE_FRAME_AVAILABLE,
    RECEIVER_SOURCE_INVALID_FRAME,
    RECEIVER_SOURCE_ERROR,
} receiver_source_result_t;

typedef receiver_source_result_t (*receiver_source_read_t)(
    void *context,
    receiver_channel_frame_t *frame);

typedef struct {
    receiver_source_read_t read;
    void *context;
} receiver_source_t;

#endif
