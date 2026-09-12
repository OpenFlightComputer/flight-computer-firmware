#include "control_trace.h"

#include <assert.h>
#include <stdint.h>

static control_trace_sample_t sample_at(uint64_t timestamp_us)
{
    return (control_trace_sample_t){
        .timestamp_us = timestamp_us,
        .system_state = SYSTEM_STATE_DISARMED,
        .control_source = MOTOR_CONTROL_SOURCE_NONE,
        .failsafe_state = RECEIVER_FAILSAFE_LIVE,
        .failsafe_action = RECEIVER_FAILSAFE_ACTION_LIVE,
        .imu_freshness = IMU_FRESHNESS_FRESH,
        .control_result = FLIGHT_CONTROL_IDLE,
        .rate_result = RATE_CONTROLLER_RESULT_DISABLED,
    };
}

static void trace_is_off_until_started(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(1000U);

    control_trace_initialize(&trace);
    assert(trace.initialized);
    assert(trace.level == CONTROL_TRACE_LEVEL_OFF);
    assert(!control_trace_record(&trace, &sample));
    assert(!control_trace_start(&trace, CONTROL_TRACE_LEVEL_OFF, 0U));
    assert(!control_trace_start(&trace, CONTROL_TRACE_LEVEL_COUNT, 0U));
    assert(control_trace_pending_count(&trace) == 0U);
}

static void event_capture_only_records_changes(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(1000U);
    control_trace_record_t records[4];
    control_trace_batch_t batch;

    control_trace_initialize(&trace);
    assert(control_trace_start(&trace, CONTROL_TRACE_LEVEL_EVENTS, 1000U));
    assert(!control_trace_start(&trace, CONTROL_TRACE_LEVEL_HIGH_RATE, 1000U));
    assert(control_trace_record(&trace, &sample));
    assert(!control_trace_record(&trace, &sample));
    sample.timestamp_us = 2000U;
    sample.failsafe_state = RECEIVER_FAILSAFE_STALE_HOLD;
    assert(control_trace_record(&trace, &sample));
    assert(control_trace_peek(&trace, records, 4U, &batch));
    assert(batch.record_count == 2U);
    assert((records[0].event_flags & CONTROL_TRACE_EVENT_CAPTURE_STARTED) != 0U);
    assert((records[1].event_flags & CONTROL_TRACE_EVENT_FAILSAFE_CHANGED) != 0U);
}

static void periodic_levels_use_their_configured_intervals(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(5000U);

    control_trace_initialize(&trace);
    assert(control_trace_start(&trace, CONTROL_TRACE_LEVEL_HIGH_RATE, 5000U));
    assert(control_trace_record(&trace, &sample));
    sample.timestamp_us = 14999U;
    assert(!control_trace_record(&trace, &sample));
    sample.timestamp_us = 15000U;
    assert(control_trace_record(&trace, &sample));
    sample.timestamp_us = 65000U;
    assert(control_trace_record(&trace, &sample));
    assert(control_trace_pending_count(&trace) == 3U);
}

static void full_buffer_drops_new_records_without_overwriting_old_ones(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(0U);
    control_trace_record_t records[CONTROL_TRACE_CAPACITY];
    control_trace_batch_t batch;
    size_t index;

    control_trace_initialize(&trace);
    assert(control_trace_start(&trace, CONTROL_TRACE_LEVEL_FULL_RATE, 0U));
    for (index = 0U; index < CONTROL_TRACE_CAPACITY; index++) {
        sample.timestamp_us = (uint64_t)index * 1000U;
        assert(control_trace_record(&trace, &sample));
    }
    sample.timestamp_us += 1000U;
    assert(!control_trace_record(&trace, &sample));
    assert(trace.dropped_record_count == 1U);
    assert(control_trace_peek(&trace, records, CONTROL_TRACE_CAPACITY, &batch));
    assert(batch.record_count == CONTROL_TRACE_CAPACITY);
    assert(records[0].sequence == 1U);
    assert(records[CONTROL_TRACE_CAPACITY - 1U].sequence ==
           CONTROL_TRACE_CAPACITY);
}

static void read_discard_and_wrap_preserve_sequence(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(0U);
    control_trace_record_t records[8];
    control_trace_batch_t batch;
    size_t index;

    control_trace_initialize(&trace);
    assert(control_trace_start(&trace, CONTROL_TRACE_LEVEL_FULL_RATE, 0U));
    for (index = 0U; index < 60U; index++) {
        sample.timestamp_us = (uint64_t)index * 1000U;
        assert(control_trace_record(&trace, &sample));
    }
    assert(control_trace_discard(&trace, 56U));
    for (index = 60U; index < 68U; index++) {
        sample.timestamp_us = (uint64_t)index * 1000U;
        assert(control_trace_record(&trace, &sample));
    }
    assert(control_trace_peek(&trace, records, 8U, &batch));
    assert(batch.record_count == 8U);
    assert(records[0].sequence == 57U);
    assert(records[7].sequence == 64U);
    assert(control_trace_discard(&trace, 8U));
    assert(control_trace_peek(&trace, records, 8U, &batch));
    assert(batch.record_count == 4U);
    assert(records[0].sequence == 65U);
}

static void capture_stops_after_armed_operation_ends(void)
{
    control_trace_t trace;
    control_trace_sample_t sample = sample_at(0U);

    control_trace_initialize(&trace);
    assert(control_trace_start(&trace, CONTROL_TRACE_LEVEL_EVENTS, 0U));
    assert(control_trace_record(&trace, &sample));
    sample.timestamp_us = 1000U;
    sample.system_state = SYSTEM_STATE_ARMED;
    assert(control_trace_record(&trace, &sample));
    assert(trace.level == CONTROL_TRACE_LEVEL_EVENTS);
    sample.timestamp_us = 2000U;
    sample.system_state = SYSTEM_STATE_FAILSAFE;
    assert(control_trace_record(&trace, &sample));
    assert(trace.level == CONTROL_TRACE_LEVEL_EVENTS);
    sample.timestamp_us = 3000U;
    sample.system_state = SYSTEM_STATE_DISARMED;
    assert(control_trace_record(&trace, &sample));
    assert(trace.level == CONTROL_TRACE_LEVEL_OFF);
    assert(control_trace_pending_count(&trace) == 4U);
}

int main(void)
{
    trace_is_off_until_started();
    event_capture_only_records_changes();
    periodic_levels_use_their_configured_intervals();
    full_buffer_drops_new_records_without_overwriting_old_ones();
    read_discard_and_wrap_preserve_sequence();
    capture_stops_after_armed_operation_ends();
    return 0;
}
