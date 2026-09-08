#include "receiver_service.h"

#include <limits.h>
#include <stddef.h>

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

bool receiver_service_initialize(receiver_service_t *service,
                                 const receiver_source_t *source,
                                 receiver_service_clock_t clock,
                                 const receiver_normalization_config_t *
                                     normalization_config,
                                 const receiver_freshness_config_t *
                                     freshness_config)
{
    receiver_normalizer_t normalizer;

    if (service == NULL) {
        return false;
    }
    *service = (receiver_service_t){0};
    if ((source == NULL) || (source->read == NULL) ||
        (clock == NULL) ||
        !receiver_freshness_config_is_valid(freshness_config) ||
        (receiver_normalizer_initialize(&normalizer, normalization_config) !=
         RECEIVER_NORMALIZATION_OK)) {
        return false;
    }

    *service = (receiver_service_t){
        .source = *source,
        .clock = clock,
        .normalizer = normalizer,
        .freshness_config = *freshness_config,
        .control.freshness = RECEIVER_FRESHNESS_UNAVAILABLE,
        .last_result = RECEIVER_SERVICE_IDLE,
        .initialized = true,
    };
    return true;
}

receiver_service_result_t receiver_service_process_once(
    receiver_service_t *service)
{
    receiver_channel_frame_t frame = {0};
    receiver_control_snapshot_t normalized;
    receiver_source_result_t source_result;
    uint64_t now_us;

    if ((service == NULL) || !service->initialized ||
        (service->source.read == NULL) || (service->clock == NULL)) {
        return RECEIVER_SERVICE_NOT_INITIALIZED;
    }

    saturating_increment(&service->statistics.poll_count);
    source_result = service->source.read(service->source.context, &frame);
    now_us = service->clock();
    switch (source_result) {
    case RECEIVER_SOURCE_NO_FRAME:
        saturating_increment(&service->statistics.no_frame_count);
        service->last_result = RECEIVER_SERVICE_IDLE;
        break;
    case RECEIVER_SOURCE_FRAME_AVAILABLE:
    {
        uint32_t sequence = service->latest.sequence;

        saturating_increment(&sequence);
        if (receiver_normalize(&service->normalizer,
                               &frame,
                               now_us,
                               sequence,
                               &normalized) != RECEIVER_NORMALIZATION_OK) {
            saturating_increment(
                &service->statistics.normalization_error_count);
            service->last_result = RECEIVER_SERVICE_NORMALIZATION_ERROR;
            break;
        }
        service->latest = (receiver_snapshot_t){
            .frame = frame,
            .received_at_us = now_us,
            .sequence = sequence,
            .valid = true,
        };
        service->control.snapshot = normalized;
        saturating_increment(&service->statistics.accepted_frame_count);
        service->last_result = RECEIVER_SERVICE_FRAME_ACCEPTED;
        break;
    }
    case RECEIVER_SOURCE_INVALID_FRAME:
        saturating_increment(&service->statistics.invalid_frame_count);
        service->last_result = RECEIVER_SERVICE_INVALID_FRAME;
        break;
    case RECEIVER_SOURCE_ERROR:
    default:
        saturating_increment(&service->statistics.source_error_count);
        service->last_result = RECEIVER_SERVICE_SOURCE_ERROR;
        break;
    }

    service->control.freshness = receiver_freshness_evaluate(
        &service->freshness_config,
        service->control.snapshot.valid,
        service->control.snapshot.received_at_us,
        now_us);

    return service->last_result;
}

bool receiver_service_latest(const receiver_service_t *service,
                             receiver_snapshot_t *snapshot)
{
    if ((service == NULL) || !service->initialized || (snapshot == NULL) ||
        !service->latest.valid) {
        return false;
    }

    *snapshot = service->latest;
    return true;
}

bool receiver_service_control_state(const receiver_service_t *service,
                                    receiver_control_state_t *state)
{
    if ((service == NULL) || !service->initialized || (state == NULL)) {
        return false;
    }

    *state = service->control;
    return true;
}

void receiver_service_task(void *context)
{
    receiver_service_t *service = context;

    if (service != NULL) {
        (void)receiver_service_process_once(service);
    }
}
