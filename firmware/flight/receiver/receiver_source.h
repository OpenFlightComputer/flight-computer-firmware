#ifndef OPENFLIGHTCOMPUTER_RECEIVER_SOURCE_H
#define OPENFLIGHTCOMPUTER_RECEIVER_SOURCE_H

#include <stdbool.h>
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
    uint32_t received_byte_count;
    uint32_t source_error;
    uint32_t source_error_count;
    uint32_t recovery_count;
    uint32_t recovery_failure_count;
    uint32_t valid_frame_count;
    uint32_t channel_frame_count;
    uint32_t link_statistics_frame_count;
    uint32_t crc_error_count;
    uint32_t framing_error_count;
    uint32_t buffer_overrun_count;
    uint32_t dropped_byte_count;
    int16_t uplink_rssi_dbm;
    uint8_t uplink_link_quality_percent;
    int8_t uplink_snr_db;
    bool link_statistics_present;
    bool recovery_pending;
} receiver_source_statistics_t;

typedef void (*receiver_source_maintain_t)(void *context);
typedef bool (*receiver_source_statistics_fn_t)(
    void *context,
    receiver_source_statistics_t *statistics);

typedef struct {
    receiver_source_read_t read;
    receiver_source_maintain_t maintain;
    receiver_source_statistics_fn_t statistics;
    void *context;
} receiver_source_t;

#endif
