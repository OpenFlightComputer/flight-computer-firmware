#ifndef OPENFLIGHTCOMPUTER_RECEIVER_INSPECTION_H
#define OPENFLIGHTCOMPUTER_RECEIVER_INSPECTION_H

#include "receiver_failsafe.h"
#include "receiver_freshness.h"
#include "receiver_source.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t channels[RECEIVER_CHANNEL_COUNT];
    uint32_t sequence;
    uint64_t age_us;
    receiver_freshness_state_t freshness;
    float roll;
    float pitch;
    float yaw;
    float throttle;
    bool arm_switch_high;
    receiver_failsafe_state_t failsafe_state;
    receiver_failsafe_action_t failsafe_action;
    bool stage_two_latched;
    bool recovery_ready;
    bool link_statistics_present;
    int16_t uplink_rssi_dbm;
    uint8_t uplink_link_quality_percent;
    int8_t uplink_snr_db;
    uint32_t uart_received_byte_count;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t framing_error_count;
    uint32_t dma_overrun_count;
    uint32_t dma_dropped_byte_count;
    bool available;
} receiver_inspection_t;

typedef bool (*receiver_inspection_read_t)(void *context,
                                           receiver_inspection_t *inspection);

typedef struct {
    receiver_inspection_read_t read;
    void *context;
} receiver_inspection_provider_t;

#endif
