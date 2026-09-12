#ifndef OPENFLIGHTCOMPUTER_CONTROL_TRACE_RECORDER_H
#define OPENFLIGHTCOMPUTER_CONTROL_TRACE_RECORDER_H

#include "flight_control.h"

void control_trace_recorder_record(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const flight_control_output_t *output,
    flight_control_result_t control_result);

#endif
