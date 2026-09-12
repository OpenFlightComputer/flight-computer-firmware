#include "control_trace.h"

#include <limits.h>
#include <stddef.h>

#define CONTROL_TRACE_LOW_RATE_INTERVAL_US UINT32_C(100000)
#define CONTROL_TRACE_HIGH_RATE_INTERVAL_US UINT32_C(10000)
#define CONTROL_TRACE_FULL_RATE_INTERVAL_US UINT32_C(1000)

static uint32_t level_interval_us(control_trace_level_t level)
{
    switch (level) {
    case CONTROL_TRACE_LEVEL_LOW_RATE:
        return CONTROL_TRACE_LOW_RATE_INTERVAL_US;
    case CONTROL_TRACE_LEVEL_HIGH_RATE:
        return CONTROL_TRACE_HIGH_RATE_INTERVAL_US;
    case CONTROL_TRACE_LEVEL_FULL_RATE:
        return CONTROL_TRACE_FULL_RATE_INTERVAL_US;
    case CONTROL_TRACE_LEVEL_OFF:
    case CONTROL_TRACE_LEVEL_EVENTS:
    case CONTROL_TRACE_LEVEL_COUNT:
        return 0U;
    }
    return 0U;
}

static void saturating_increment_u64(uint64_t *value)
{
    if (*value < UINT64_MAX) {
        (*value)++;
    }
}

void control_trace_initialize(control_trace_t *trace)
{
    if (trace == NULL) {
        return;
    }
    *trace = (control_trace_t){
        .level = CONTROL_TRACE_LEVEL_OFF,
        .initialized = true,
    };
}

bool control_trace_start(control_trace_t *trace,
                         control_trace_level_t level,
                         uint64_t now_us)
{
    uint32_t capture_id;

    if ((trace == NULL) || !trace->initialized ||
        (trace->level != CONTROL_TRACE_LEVEL_OFF) ||
        (level <= CONTROL_TRACE_LEVEL_OFF) ||
        (level >= CONTROL_TRACE_LEVEL_COUNT)) {
        return false;
    }
    capture_id = trace->capture_id;
    if (capture_id < UINT32_MAX) {
        capture_id++;
    }
    *trace = (control_trace_t){
        .capture_id = capture_id,
        .sampling_interval_us = level_interval_us(level),
        .level = level,
        .next_sample_at_us = now_us,
        .initialized = true,
    };
    return true;
}

void control_trace_stop(control_trace_t *trace)
{
    if ((trace != NULL) && trace->initialized) {
        trace->level = CONTROL_TRACE_LEVEL_OFF;
        trace->sampling_interval_us = 0U;
    }
}

static uint32_t observe_events(control_trace_t *trace,
                               const control_trace_sample_t *sample)
{
    uint32_t events = 0U;

    if (!trace->observation_valid) {
        events = CONTROL_TRACE_EVENT_CAPTURE_STARTED;
    } else {
        if (sample->system_state != trace->previous_system_state) {
            events |= CONTROL_TRACE_EVENT_STATE_CHANGED;
        }
        if (sample->failsafe_state != trace->previous_failsafe_state) {
            events |= CONTROL_TRACE_EVENT_FAILSAFE_CHANGED;
        }
        if (sample->rate_result != trace->previous_rate_result) {
            events |= CONTROL_TRACE_EVENT_RATE_RESULT_CHANGED;
        }
        if (sample->control_result != trace->previous_control_result) {
            events |= CONTROL_TRACE_EVENT_CONTROL_RESULT_CHANGED;
        }
        if (sample->mixer_output_valid && sample->mixer_output.saturated &&
            !trace->previous_saturated) {
            events |= CONTROL_TRACE_EVENT_SATURATION_STARTED;
        }
    }
    trace->previous_system_state = sample->system_state;
    trace->previous_failsafe_state = sample->failsafe_state;
    trace->previous_rate_result = sample->rate_result;
    trace->previous_control_result = sample->control_result;
    trace->previous_saturated = sample->mixer_output_valid &&
                                sample->mixer_output.saturated;
    trace->observation_valid = true;
    if (sample->system_state == SYSTEM_STATE_ARMED) {
        trace->armed_seen = true;
    }
    return events;
}

static bool periodic_sample_is_due(control_trace_t *trace, uint64_t now_us)
{
    if ((trace->sampling_interval_us == 0U) ||
        (now_us < trace->next_sample_at_us)) {
        return false;
    }
    do {
        const uint64_t remaining = UINT64_MAX - trace->next_sample_at_us;

        if (remaining < trace->sampling_interval_us) {
            trace->next_sample_at_us = UINT64_MAX;
            break;
        }
        trace->next_sample_at_us += trace->sampling_interval_us;
    } while (trace->next_sample_at_us <= now_us);
    return true;
}

static void copy_record(control_trace_record_t *record,
                        const control_trace_sample_t *sample,
                        uint64_t sequence,
                        uint32_t events)
{
    size_t axis;
    size_t motor;

    *record = (control_trace_record_t){
        .sequence = sequence,
        .timestamp_us = sample->timestamp_us,
        .imu_sequence = sample->attitude.source_sequence,
        .event_flags = events,
        .dt_us = sample->rate_output.dt_us,
        .system_state = sample->system_state,
        .control_source = sample->control_source,
        .failsafe_state = sample->failsafe_state,
        .failsafe_action = sample->failsafe_action,
        .imu_freshness = sample->imu_freshness,
        .control_result = sample->control_result,
        .rate_result = sample->rate_result,
        .receiver = {
            sample->receiver.throttle,
            sample->receiver.roll,
            sample->receiver.pitch,
            sample->receiver.yaw,
        },
        .setpoint = {
            sample->setpoint.throttle,
            sample->setpoint.desired_roll_degrees,
            sample->setpoint.desired_pitch_degrees,
            sample->setpoint.desired_yaw_rate_dps,
        },
        .attitude = {
            sample->attitude.roll_degrees,
            sample->attitude.pitch_degrees,
        },
        .mixer_scale = sample->mixer_output.correction_scale,
        .collective_shift = sample->mixer_output.collective_shift,
        .receiver_valid = sample->receiver.valid,
        .setpoint_valid = sample->setpoint.valid,
        .attitude_valid = sample->attitude.valid,
        .rate_output_valid = sample->rate_output.valid,
        .mixer_output_valid = sample->mixer_output_valid,
        .mixer_saturated = sample->mixer_output_valid &&
                           sample->mixer_output.saturated,
    };
    for (axis = 0U; axis < 3U; axis++) {
        record->desired_rate[axis] = sample->desired_rates.desired_rate_dps[axis];
        record->measured_rate[axis] =
            sample->attitude.filtered_gyroscope_dps[axis];
        record->pid[axis] = sample->rate_output.axis[axis];
    }
    if (sample->mixer_output_valid) {
        for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            record->motor[motor] =
                sample->mixer_output.command.throttle[motor];
        }
    }
}

bool control_trace_record(control_trace_t *trace,
                          const control_trace_sample_t *sample)
{
    uint32_t events;
    bool periodic;
    bool stop_after_record;

    if ((trace == NULL) || !trace->initialized ||
        (sample == NULL) || (trace->level == CONTROL_TRACE_LEVEL_OFF)) {
        return false;
    }
    events = observe_events(trace, sample);
    periodic = periodic_sample_is_due(trace, sample->timestamp_us);
    stop_after_record = (sample->system_state == SYSTEM_STATE_FAULT) ||
                        (trace->armed_seen &&
                         (sample->system_state == SYSTEM_STATE_DISARMED));
    if (!periodic && (events == 0U) && !stop_after_record) {
        return false;
    }
    if ((trace->producer_count - trace->consumer_count) >=
        CONTROL_TRACE_CAPACITY) {
        saturating_increment_u64(&trace->dropped_record_count);
        if (stop_after_record) {
            control_trace_stop(trace);
        }
        return false;
    }
    copy_record(&trace->records[trace->producer_count % CONTROL_TRACE_CAPACITY],
                sample, trace->producer_count + 1U, events);
    trace->producer_count++;
    if (stop_after_record) {
        control_trace_stop(trace);
    }
    return true;
}

size_t control_trace_pending_count(const control_trace_t *trace)
{
    uint64_t pending;

    if ((trace == NULL) || !trace->initialized ||
        (trace->producer_count < trace->consumer_count)) {
        return 0U;
    }
    pending = trace->producer_count - trace->consumer_count;
    return pending > SIZE_MAX ? SIZE_MAX : (size_t)pending;
}

bool control_trace_peek(const control_trace_t *trace,
                        control_trace_record_t *records,
                        size_t capacity,
                        control_trace_batch_t *batch)
{
    size_t available;
    size_t count;
    size_t index;

    if ((trace == NULL) || !trace->initialized || (records == NULL) ||
        (capacity == 0U) || (batch == NULL)) {
        return false;
    }
    available = control_trace_pending_count(trace);
    count = available < capacity ? available : capacity;
    for (index = 0U; index < count; index++) {
        records[index] = trace->records[
            (trace->consumer_count + index) % CONTROL_TRACE_CAPACITY];
    }
    *batch = (control_trace_batch_t){
        .capture_id = trace->capture_id,
        .level = trace->level,
        .first_sequence = count > 0U ? records[0].sequence
                                     : trace->consumer_count + 1U,
        .next_sequence = trace->consumer_count + count + 1U,
        .dropped_record_count = trace->dropped_record_count,
        .record_count = count,
        .capturing = trace->level != CONTROL_TRACE_LEVEL_OFF,
    };
    return true;
}

bool control_trace_discard(control_trace_t *trace, size_t count)
{
    if ((trace == NULL) || !trace->initialized ||
        (count > control_trace_pending_count(trace))) {
        return false;
    }
    trace->consumer_count += count;
    return true;
}

const char *control_trace_level_name(control_trace_level_t level)
{
    switch (level) {
    case CONTROL_TRACE_LEVEL_OFF:
        return "OFF";
    case CONTROL_TRACE_LEVEL_EVENTS:
        return "EVENTS";
    case CONTROL_TRACE_LEVEL_LOW_RATE:
        return "LOW_RATE";
    case CONTROL_TRACE_LEVEL_HIGH_RATE:
        return "HIGH_RATE";
    case CONTROL_TRACE_LEVEL_FULL_RATE:
        return "FULL_RATE";
    case CONTROL_TRACE_LEVEL_COUNT:
        break;
    }
    return "INVALID";
}
