#ifndef OPENFLIGHTCOMPUTER_FLIGHT_DIAGNOSTICS_H
#define OPENFLIGHTCOMPUTER_FLIGHT_DIAGNOSTICS_H

#include "flight_control.h"

void flight_diagnostics_capture(
    uint64_t now_us,
    const receiver_failsafe_decision_t *decision,
    const attitude_snapshot_t *attitude,
    const receiver_flight_control_output_t *output,
    flight_control_result_t control_result);

#endif
