#ifndef OPENFLIGHTCOMPUTER_CONTROL_TRACE_H
#define OPENFLIGHTCOMPUTER_CONTROL_TRACE_H

#include "flight_control.h"
#include "motor_control.h"
#include "system_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONTROL_TRACE_CAPACITY 64U
#define CONTROL_TRACE_USB_RECORD_LIMIT 10U

typedef enum {
    CONTROL_TRACE_LEVEL_OFF = 0,
    CONTROL_TRACE_LEVEL_EVENTS,
    CONTROL_TRACE_LEVEL_LOW_RATE,
    CONTROL_TRACE_LEVEL_HIGH_RATE,
    CONTROL_TRACE_LEVEL_FULL_RATE,
    CONTROL_TRACE_LEVEL_COUNT,
} control_trace_level_t;

enum {
    CONTROL_TRACE_EVENT_STATE_CHANGED = UINT32_C(1) << 0,
    CONTROL_TRACE_EVENT_FAILSAFE_CHANGED = UINT32_C(1) << 1,
    CONTROL_TRACE_EVENT_RATE_RESULT_CHANGED = UINT32_C(1) << 2,
    CONTROL_TRACE_EVENT_CONTROL_RESULT_CHANGED = UINT32_C(1) << 3,
    CONTROL_TRACE_EVENT_SATURATION_STARTED = UINT32_C(1) << 4,
    CONTROL_TRACE_EVENT_CAPTURE_STARTED = UINT32_C(1) << 5,
};

typedef struct {
    uint64_t timestamp_us;
    system_state_t system_state;
    motor_control_source_t control_source;
    receiver_failsafe_state_t failsafe_state;
    receiver_failsafe_action_t failsafe_action;
    imu_freshness_t imu_freshness;
    flight_control_result_t control_result;
    rate_controller_result_t rate_result;
    receiver_control_snapshot_t receiver;
    control_setpoint_t setpoint;
    attitude_snapshot_t attitude;
    flight_control_desired_rates_t desired_rates;
    rate_controller_output_t rate_output;
    quad_x_mixer_output_t mixer_output;
    bool mixer_output_valid;
} control_trace_sample_t;

typedef struct {
    uint64_t sequence;
    uint64_t timestamp_us;
    uint64_t imu_sequence;
    uint32_t event_flags;
    uint32_t dt_us;
    system_state_t system_state;
    motor_control_source_t control_source;
    receiver_failsafe_state_t failsafe_state;
    receiver_failsafe_action_t failsafe_action;
    imu_freshness_t imu_freshness;
    flight_control_result_t control_result;
    rate_controller_result_t rate_result;
    float receiver[4];
    float setpoint[4];
    float attitude[2];
    float desired_rate[3];
    float measured_rate[3];
    rate_pid_output_t pid[3];
    float motor[4];
    float mixer_scale;
    float collective_shift;
    bool receiver_valid;
    bool setpoint_valid;
    bool attitude_valid;
    bool rate_output_valid;
    bool mixer_output_valid;
    bool mixer_saturated;
} control_trace_record_t;

typedef struct {
    control_trace_record_t records[CONTROL_TRACE_CAPACITY];
    uint64_t producer_count;
    uint64_t consumer_count;
    uint64_t dropped_record_count;
    uint64_t next_sample_at_us;
    uint32_t capture_id;
    uint32_t sampling_interval_us;
    control_trace_level_t level;
    system_state_t previous_system_state;
    receiver_failsafe_state_t previous_failsafe_state;
    rate_controller_result_t previous_rate_result;
    flight_control_result_t previous_control_result;
    bool previous_saturated;
    bool observation_valid;
    bool armed_seen;
    bool initialized;
} control_trace_t;

typedef struct {
    uint32_t capture_id;
    control_trace_level_t level;
    uint64_t first_sequence;
    uint64_t next_sequence;
    uint64_t dropped_record_count;
    size_t record_count;
    bool capturing;
} control_trace_batch_t;

void control_trace_initialize(control_trace_t *trace);
bool control_trace_start(control_trace_t *trace,
                         control_trace_level_t level,
                         uint64_t now_us);
void control_trace_stop(control_trace_t *trace);
bool control_trace_record(control_trace_t *trace,
                          const control_trace_sample_t *sample);
bool control_trace_peek(const control_trace_t *trace,
                        control_trace_record_t *records,
                        size_t capacity,
                        control_trace_batch_t *batch);
bool control_trace_discard(control_trace_t *trace, size_t count);
size_t control_trace_pending_count(const control_trace_t *trace);
const char *control_trace_level_name(control_trace_level_t level);

#endif
