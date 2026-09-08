#include "receiver_service.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

typedef struct {
    receiver_source_result_t result;
    receiver_channel_frame_t frame;
    uint32_t read_count;
} fake_source_t;

static uint64_t fake_now_us;
static receiver_normalization_config_t normalization_config;
static const receiver_freshness_config_t freshness_config = {
    .fresh_through_us = 100U,
    .lost_after_us = 300U,
};

static uint64_t fake_clock(void)
{
    return fake_now_us;
}

static receiver_source_result_t fake_read(
    void *context,
    receiver_channel_frame_t *frame)
{
    fake_source_t *source = context;

    source->read_count++;
    if (source->result == RECEIVER_SOURCE_FRAME_AVAILABLE) {
        *frame = source->frame;
    }
    return source->result;
}

static receiver_source_t source_for(fake_source_t *fake)
{
    const receiver_source_t source = {
        .read = fake_read,
        .context = fake,
    };

    return source;
}

static bool initialize_service(receiver_service_t *service,
                               const receiver_source_t *source)
{
    receiver_normalization_default_config(&normalization_config);
    return receiver_service_initialize(service,
                                       source,
                                       fake_clock,
                                       &normalization_config,
                                       &freshness_config);
}

static void test_initialization_rejects_invalid_dependencies(void)
{
    receiver_service_t service;
    fake_source_t fake = {0};
    receiver_source_t source = source_for(&fake);

    receiver_normalization_default_config(&normalization_config);
    assert(!receiver_service_initialize(NULL,
                                        &source,
                                        fake_clock,
                                        &normalization_config,
                                        &freshness_config));
    assert(!receiver_service_initialize(&service,
                                        NULL,
                                        fake_clock,
                                        &normalization_config,
                                        &freshness_config));
    source.read = NULL;
    assert(!receiver_service_initialize(&service,
                                        &source,
                                        fake_clock,
                                        &normalization_config,
                                        &freshness_config));
    source = source_for(&fake);
    assert(!receiver_service_initialize(&service,
                                        &source,
                                        NULL,
                                        &normalization_config,
                                        &freshness_config));
    assert(!receiver_service_initialize(&service,
                                        &source,
                                        fake_clock,
                                        NULL,
                                        &freshness_config));
    assert(!receiver_service_initialize(&service,
                                        &source,
                                        fake_clock,
                                        &normalization_config,
                                        NULL));
    assert(receiver_service_process_once(NULL) ==
           RECEIVER_SERVICE_NOT_INITIALIZED);
    service = (receiver_service_t){0};
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_NOT_INITIALIZED);
}

static void test_no_frame_is_bounded_and_preserves_invalid_snapshot(void)
{
    receiver_service_t service;
    receiver_snapshot_t snapshot;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_NO_FRAME,
    };
    receiver_source_t source = source_for(&fake);

    assert(initialize_service(&service, &source));
    assert(receiver_service_process_once(&service) == RECEIVER_SERVICE_IDLE);
    assert(fake.read_count == 1U);
    assert(service.statistics.poll_count == 1U);
    assert(service.statistics.no_frame_count == 1U);
    assert(!receiver_service_latest(&service, &snapshot));
    assert(!receiver_service_latest(&service, NULL));
}

static void test_complete_frame_is_copied_timestamped_and_replaced(void)
{
    receiver_service_t service;
    receiver_snapshot_t snapshot;
    receiver_control_state_t control;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_FRAME_AVAILABLE,
    };
    receiver_source_t source = source_for(&fake);
    size_t channel;

    for (channel = 0U; channel < RECEIVER_CHANNEL_COUNT; channel++) {
        fake.frame.channels[channel] = (uint16_t)(100U + channel);
    }
    fake_now_us = 1234U;
    fake.frame.channels[0] = 992U;
    fake.frame.channels[1] = 1811U;
    fake.frame.channels[2] = 172U;
    fake.frame.channels[3] = 172U;
    fake.frame.channels[4] = 1500U;
    assert(initialize_service(&service, &source));
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);

    fake.frame.channels[0] = 999U;
    assert(receiver_service_latest(&service, &snapshot));
    assert(snapshot.valid);
    assert(snapshot.sequence == 1U);
    assert(snapshot.received_at_us == 1234U);
    assert(snapshot.frame.channels[0] == 992U);
    assert(snapshot.frame.channels[15] == 115U);
    assert(receiver_service_control_state(&service, &control));
    assert(control.snapshot.valid);
    assert(control.snapshot.roll == 0.0f);
    assert(control.snapshot.pitch == 1.0f);
    assert(control.snapshot.yaw == -1.0f);
    assert(control.snapshot.throttle == 0.0f);
    assert(control.snapshot.arm_switch_high);
    assert(control.snapshot.received_at_us == snapshot.received_at_us);
    assert(control.snapshot.source_sequence == snapshot.sequence);
    assert(control.freshness == RECEIVER_FRESHNESS_FRESH);

    fake_now_us = 4321U;
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);
    assert(receiver_service_latest(&service, &snapshot));
    assert(snapshot.sequence == 2U);
    assert(snapshot.received_at_us == 4321U);
    assert(snapshot.frame.channels[0] == 999U);
    assert(service.statistics.accepted_frame_count == 2U);
}

static void test_invalid_and_error_results_preserve_last_good_frame(void)
{
    receiver_service_t service;
    receiver_snapshot_t before;
    receiver_snapshot_t after;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_FRAME_AVAILABLE,
        .frame.channels = {321U},
    };
    receiver_source_t source = source_for(&fake);

    fake_now_us = 55U;
    assert(initialize_service(&service, &source));
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);
    assert(receiver_service_latest(&service, &before));

    fake.result = RECEIVER_SOURCE_INVALID_FRAME;
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_INVALID_FRAME);
    fake.result = RECEIVER_SOURCE_ERROR;
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_SOURCE_ERROR);
    fake.result = (receiver_source_result_t)99;
    receiver_service_task(&service);

    assert(receiver_service_latest(&service, &after));
    assert(after.sequence == before.sequence);
    assert(after.received_at_us == before.received_at_us);
    assert(after.frame.channels[0] == before.frame.channels[0]);
    assert(service.statistics.invalid_frame_count == 1U);
    assert(service.statistics.source_error_count == 2U);
    assert(service.statistics.poll_count == 4U);
    assert(service.last_result == RECEIVER_SERVICE_SOURCE_ERROR);
}

static void test_freshness_advances_without_replacing_snapshot(void)
{
    receiver_service_t service;
    receiver_control_state_t control;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_FRAME_AVAILABLE,
        .frame.channels = {992U, 992U, 172U, 992U},
    };
    receiver_source_t source = source_for(&fake);

    fake_now_us = 1000U;
    assert(initialize_service(&service, &source));
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);

    fake.result = RECEIVER_SOURCE_NO_FRAME;
    fake_now_us = 1100U;
    assert(receiver_service_process_once(&service) == RECEIVER_SERVICE_IDLE);
    assert(receiver_service_control_state(&service, &control));
    assert(control.freshness == RECEIVER_FRESHNESS_FRESH);

    fake_now_us = 1101U;
    assert(receiver_service_process_once(&service) == RECEIVER_SERVICE_IDLE);
    assert(receiver_service_control_state(&service, &control));
    assert(control.freshness == RECEIVER_FRESHNESS_STALE);

    fake_now_us = 1301U;
    assert(receiver_service_process_once(&service) == RECEIVER_SERVICE_IDLE);
    assert(receiver_service_control_state(&service, &control));
    assert(control.freshness == RECEIVER_FRESHNESS_LOST);
    assert(control.snapshot.received_at_us == 1000U);

    fake_now_us = 999U;
    assert(receiver_service_process_once(&service) == RECEIVER_SERVICE_IDLE);
    assert(receiver_service_control_state(&service, &control));
    assert(control.freshness == RECEIVER_FRESHNESS_LOST);
}

static void test_normalization_failure_preserves_last_snapshot(void)
{
    receiver_service_t service;
    receiver_snapshot_t before;
    receiver_snapshot_t after;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_FRAME_AVAILABLE,
    };
    receiver_source_t source = source_for(&fake);

    fake_now_us = 10U;
    assert(initialize_service(&service, &source));
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);
    assert(receiver_service_latest(&service, &before));

    service.normalizer.initialized = false;
    fake_now_us = 20U;
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_NORMALIZATION_ERROR);
    assert(receiver_service_latest(&service, &after));
    assert(after.sequence == before.sequence);
    assert(after.received_at_us == before.received_at_us);
    assert(service.statistics.normalization_error_count == 1U);
}

static void test_counters_saturate(void)
{
    receiver_service_t service;
    fake_source_t fake = {
        .result = RECEIVER_SOURCE_FRAME_AVAILABLE,
    };
    receiver_source_t source = source_for(&fake);

    assert(initialize_service(&service, &source));
    service.statistics.poll_count = UINT32_MAX;
    service.statistics.accepted_frame_count = UINT32_MAX;
    service.statistics.normalization_error_count = UINT32_MAX;
    service.latest.sequence = UINT32_MAX;
    assert(receiver_service_process_once(&service) ==
           RECEIVER_SERVICE_FRAME_ACCEPTED);
    assert(service.statistics.poll_count == UINT32_MAX);
    assert(service.statistics.accepted_frame_count == UINT32_MAX);
    assert(service.latest.sequence == UINT32_MAX);
}

int main(void)
{
    test_initialization_rejects_invalid_dependencies();
    test_no_frame_is_bounded_and_preserves_invalid_snapshot();
    test_complete_frame_is_copied_timestamped_and_replaced();
    test_invalid_and_error_results_preserve_last_good_frame();
    test_freshness_advances_without_replacing_snapshot();
    test_normalization_failure_preserves_last_snapshot();
    test_counters_saturate();
    return 0;
}
