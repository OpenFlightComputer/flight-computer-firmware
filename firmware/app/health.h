#ifndef OPENFLIGHTCOMPUTER_HEALTH_H
#define OPENFLIGHTCOMPUTER_HEALTH_H

#include "fault.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HEALTH_STATE_OK = 0,
    HEALTH_STATE_WARNING,
    HEALTH_STATE_DEGRADED,
    HEALTH_STATE_UNKNOWN,
    HEALTH_STATE_CRITICAL,
    HEALTH_STATE_COUNT,
} health_state_t;

typedef struct {
    health_state_t state;
    size_t active_fault_count;
    size_t warning_count;
    size_t fault_count;
    size_t critical_count;
    uint32_t dropped_fault_count;
    bool fault_data_complete;
} health_summary_t;

typedef enum {
    HEALTH_EVALUATE_OK = 0,
    HEALTH_EVALUATE_INVALID_ARGUMENT,
} health_evaluate_result_t;

health_evaluate_result_t health_evaluate(const fault_system_t *fault_system,
                                         health_summary_t *summary);
const char *health_state_name(health_state_t state);

#endif
