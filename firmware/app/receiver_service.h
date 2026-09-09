#ifndef OPENFLIGHTCOMPUTER_RECEIVER_SERVICE_H
#define OPENFLIGHTCOMPUTER_RECEIVER_SERVICE_H

#include "receiver_freshness.h"
#include "receiver_normalization.h"
#include "receiver_source.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t (*receiver_service_clock_t)(void);

typedef struct {
    receiver_channel_frame_t frame;
    uint64_t received_at_us;
    uint32_t sequence;
    bool valid;
} receiver_snapshot_t;

typedef struct {
    receiver_control_snapshot_t snapshot;
    receiver_freshness_state_t freshness;
} receiver_control_state_t;

typedef struct {
    uint32_t poll_count;
    uint32_t no_frame_count;
    uint32_t accepted_frame_count;
    uint32_t invalid_frame_count;
    uint32_t source_error_count;
    uint32_t normalization_error_count;
} receiver_service_statistics_t;

typedef enum {
    RECEIVER_SERVICE_IDLE = 0,
    RECEIVER_SERVICE_FRAME_ACCEPTED,
    RECEIVER_SERVICE_INVALID_FRAME,
    RECEIVER_SERVICE_SOURCE_ERROR,
    RECEIVER_SERVICE_NORMALIZATION_ERROR,
    RECEIVER_SERVICE_NOT_INITIALIZED,
} receiver_service_result_t;

typedef struct {
    receiver_source_t source;
    receiver_service_clock_t clock;
    receiver_normalizer_t normalizer;
    receiver_freshness_config_t freshness_config;
    receiver_snapshot_t latest;
    receiver_control_state_t control;
    receiver_service_statistics_t statistics;
    receiver_service_result_t last_result;
    bool initialized;
} receiver_service_t;

bool receiver_service_initialize(receiver_service_t *service,
                                 const receiver_source_t *source,
                                 receiver_service_clock_t clock,
                                 const receiver_normalization_config_t *
                                     normalization_config,
                                 const receiver_freshness_config_t *
                                     freshness_config);
receiver_service_result_t receiver_service_process_once(
    receiver_service_t *service);
bool receiver_service_latest(const receiver_service_t *service,
                             receiver_snapshot_t *snapshot);
bool receiver_service_control_state(const receiver_service_t *service,
                                    receiver_control_state_t *state);
bool receiver_service_update_freshness_config(
    receiver_service_t *service,
    const receiver_freshness_config_t *config);
void receiver_service_task(void *context);

#endif
