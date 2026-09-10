#include "imu_service.h"

#include <limits.h>
#include <stddef.h>

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

static void saturating_increment_u64(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        (*value)++;
    }
}

bool imu_service_initialize(imu_service_t *service,
                            const imu_source_t *source,
                            imu_service_clock_t clock,
                            const imu_axis_mapping_t *axis_mapping,
                            const imu_freshness_config_t *freshness_config)
{
    if (service == NULL) {
        return false;
    }
    *service = (imu_service_t){0};
    if ((source == NULL) || (source->read == NULL) || (clock == NULL) ||
        !imu_axis_mapping_is_valid(axis_mapping) ||
        !imu_freshness_config_is_valid(freshness_config)) {
        return false;
    }

    *service = (imu_service_t){
        .source = *source,
        .clock = clock,
        .axis_mapping = *axis_mapping,
        .freshness_config = *freshness_config,
        .initialized_at_us = clock(),
        .last_result = IMU_SERVICE_SOURCE_ERROR,
        .initialized = true,
    };
    return true;
}

imu_service_result_t imu_service_process_once(imu_service_t *service)
{
    imu_raw_sample_t raw = {0};
    imu_sample_snapshot_t next = {0};

    if ((service == NULL) || !service->initialized ||
        (service->source.read == NULL) || (service->clock == NULL)) {
        return IMU_SERVICE_NOT_INITIALIZED;
    }

    saturating_increment(&service->statistics.read_count);
    if (service->source.read(service->source.context, &raw) !=
        IMU_SOURCE_SAMPLE_AVAILABLE) {
        saturating_increment(&service->statistics.source_error_count);
        service->last_result = IMU_SERVICE_SOURCE_ERROR;
        return service->last_result;
    }
    if (!imu_map_raw_sample(&service->axis_mapping, &raw, &next)) {
        saturating_increment(&service->statistics.source_error_count);
        service->last_result = IMU_SERVICE_SOURCE_ERROR;
        return service->last_result;
    }

    next.acquired_at_us = service->clock();
    next.sequence = service->latest.sequence;
    saturating_increment_u64(&next.sequence);
    service->latest = next;
    saturating_increment(&service->statistics.published_sample_count);
    service->last_result = IMU_SERVICE_SAMPLE_PUBLISHED;
    return service->last_result;
}

bool imu_service_state(const imu_service_t *service,
                       imu_service_state_t *state)
{
    uint64_t age_us;
    imu_freshness_t freshness;

    if ((service == NULL) || !service->initialized || (state == NULL) ||
        (service->clock == NULL)) {
        return false;
    }

    {
        const uint64_t now_us = service->clock();

        freshness = imu_freshness_evaluate(&service->freshness_config,
                                           &service->latest,
                                           now_us,
                                           &age_us);
        if (!service->latest.valid &&
            ((now_us < service->initialized_at_us) ||
             ((now_us - service->initialized_at_us) >
              service->freshness_config.lost_after_us))) {
            freshness = IMU_FRESHNESS_LOST;
        }
    }
    *state = (imu_service_state_t){
        .snapshot = service->latest,
        .freshness = freshness,
        .age_us = age_us,
    };
    return true;
}
