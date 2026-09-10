#include "imu_service.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    imu_source_result_t result;
    imu_raw_sample_t sample;
    uint32_t read_count;
} fake_source_t;

static uint64_t fake_time_us;

static uint64_t fake_clock(void)
{
    return fake_time_us;
}

static imu_source_result_t fake_read(void *context, imu_raw_sample_t *sample)
{
    fake_source_t *source = context;

    source->read_count++;
    if (source->result == IMU_SOURCE_SAMPLE_AVAILABLE) {
        *sample = source->sample;
    }
    return source->result;
}

static void initializes_and_publishes_owned_samples(void)
{
    fake_source_t fake = {
        .result = IMU_SOURCE_SAMPLE_AVAILABLE,
        .sample = {
            .acceleration_x = 1,
            .acceleration_y = 2,
            .acceleration_z = 3,
            .gyroscope_x = 4,
            .gyroscope_y = 5,
            .gyroscope_z = 6,
        },
    };
    const imu_source_t source = {.read = fake_read, .context = &fake};
    const imu_axis_mapping_t mapping = {
        .body_x = IMU_AXIS_POSITIVE_Y,
        .body_y = IMU_AXIS_POSITIVE_X,
        .body_z = IMU_AXIS_NEGATIVE_Z,
    };
    const imu_freshness_config_t freshness = {
        .fresh_through_us = 2000U,
        .lost_after_us = 10000U,
    };
    imu_service_t service;
    imu_service_state_t state;

    fake_time_us = 500U;
    assert(imu_service_initialize(&service,
                                  &source,
                                  fake_clock,
                                  &mapping,
                                  &freshness));
    assert(imu_service_state(&service, &state));
    assert(!state.snapshot.valid);
    assert(state.freshness == IMU_FRESHNESS_UNAVAILABLE);
    assert(state.age_us == UINT64_MAX);

    fake_time_us = 10501U;
    assert(imu_service_state(&service, &state));
    assert(state.freshness == IMU_FRESHNESS_LOST);

    assert(imu_service_process_once(&service) ==
           IMU_SERVICE_SAMPLE_PUBLISHED);
    assert(fake.read_count == 1U);
    assert(service.statistics.read_count == 1U);
    assert(service.statistics.published_sample_count == 1U);
    assert(service.statistics.source_error_count == 0U);
    assert(imu_service_state(&service, &state));
    assert(state.snapshot.valid);
    assert(state.snapshot.sequence == 1U);
    assert(state.snapshot.acquired_at_us == 10501U);
    assert(state.snapshot.acceleration_x == 2);
    assert(state.snapshot.acceleration_y == 1);
    assert(state.snapshot.acceleration_z == -3);
    assert(state.snapshot.gyroscope_x == 5);
    assert(state.snapshot.gyroscope_y == 4);
    assert(state.snapshot.gyroscope_z == -6);
    assert(state.freshness == IMU_FRESHNESS_FRESH);
    assert(state.age_us == 0U);

    fake.sample.acceleration_y = 20;
    fake_time_us = 11501U;
    assert(imu_service_process_once(&service) ==
           IMU_SERVICE_SAMPLE_PUBLISHED);
    assert(imu_service_state(&service, &state));
    assert(state.snapshot.sequence == 2U);
    assert(state.snapshot.acceleration_x == 20);
}

static void retains_last_good_sample_across_errors(void)
{
    fake_source_t fake = {
        .result = IMU_SOURCE_SAMPLE_AVAILABLE,
        .sample.acceleration_x = 7,
    };
    const imu_source_t source = {.read = fake_read, .context = &fake};
    const imu_axis_mapping_t mapping = {
        .body_x = IMU_AXIS_POSITIVE_X,
        .body_y = IMU_AXIS_POSITIVE_Y,
        .body_z = IMU_AXIS_POSITIVE_Z,
    };
    const imu_freshness_config_t freshness = {
        .fresh_through_us = 2000U,
        .lost_after_us = 10000U,
    };
    imu_service_t service;
    imu_service_state_t state;

    fake_time_us = 100U;
    assert(imu_service_initialize(&service,
                                  &source,
                                  fake_clock,
                                  &mapping,
                                  &freshness));
    assert(imu_service_process_once(&service) ==
           IMU_SERVICE_SAMPLE_PUBLISHED);
    fake.result = IMU_SOURCE_ERROR;
    fake_time_us = 2201U;
    assert(imu_service_process_once(&service) == IMU_SERVICE_SOURCE_ERROR);
    assert(imu_service_state(&service, &state));
    assert(state.snapshot.sequence == 1U);
    assert(state.snapshot.acceleration_x == 7);
    assert(state.freshness == IMU_FRESHNESS_STALE);
    assert(state.age_us == 2101U);
    assert(service.statistics.source_error_count == 1U);

    fake_time_us = 10101U;
    assert(imu_service_state(&service, &state));
    assert(state.freshness == IMU_FRESHNESS_LOST);
    assert(state.age_us == 10001U);
}

static void rejects_invalid_initialization_and_saturates(void)
{
    fake_source_t fake = {.result = IMU_SOURCE_SAMPLE_AVAILABLE};
    imu_source_t source = {.read = fake_read, .context = &fake};
    imu_axis_mapping_t mapping = {
        .body_x = IMU_AXIS_POSITIVE_X,
        .body_y = IMU_AXIS_POSITIVE_Y,
        .body_z = IMU_AXIS_POSITIVE_Z,
    };
    imu_freshness_config_t freshness = {
        .fresh_through_us = 2000U,
        .lost_after_us = 10000U,
    };
    imu_service_t service;

    assert(!imu_service_initialize(NULL,
                                   &source,
                                   fake_clock,
                                   &mapping,
                                   &freshness));
    source.read = NULL;
    assert(!imu_service_initialize(&service,
                                   &source,
                                   fake_clock,
                                   &mapping,
                                   &freshness));
    source.read = fake_read;
    mapping.body_z = IMU_AXIS_POSITIVE_X;
    assert(!imu_service_initialize(&service,
                                   &source,
                                   fake_clock,
                                   &mapping,
                                   &freshness));
    assert(imu_service_process_once(NULL) == IMU_SERVICE_NOT_INITIALIZED);

    mapping.body_z = IMU_AXIS_POSITIVE_Z;
    assert(imu_service_initialize(&service,
                                  &source,
                                  fake_clock,
                                  &mapping,
                                  &freshness));
    service.latest.sequence = UINT64_MAX;
    service.statistics.read_count = UINT32_MAX;
    service.statistics.published_sample_count = UINT32_MAX;
    assert(imu_service_process_once(&service) ==
           IMU_SERVICE_SAMPLE_PUBLISHED);
    assert(service.latest.sequence == UINT64_MAX);
    assert(service.statistics.read_count == UINT32_MAX);
    assert(service.statistics.published_sample_count == UINT32_MAX);
}

int main(void)
{
    initializes_and_publishes_owned_samples();
    retains_last_good_sample_across_errors();
    rejects_invalid_initialization_and_saturates();
    return 0;
}
