#include "health.h"

#include <stddef.h>

static bool lifecycle_state_is_valid(system_state_t state)
{
    return (unsigned int)state < (unsigned int)SYSTEM_STATE_COUNT;
}

health_evaluate_result_t health_evaluate(const fault_system_t *fault_system,
                                         health_summary_t *summary)
{
    size_t index;

    if ((fault_system == NULL) || !fault_system->initialized ||
        (fault_system->state_machine == NULL) ||
        !fault_system->state_machine->initialized ||
        !lifecycle_state_is_valid(fault_system->state_machine->current) ||
        (summary == NULL)) {
        return HEALTH_EVALUATE_INVALID_ARGUMENT;
    }

    *summary = (health_summary_t){
        .state = HEALTH_STATE_OK,
        .dropped_fault_count = fault_system->dropped_record_count,
        .fault_data_complete = fault_system->dropped_record_count == 0U,
    };

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        const fault_record_t *record = &fault_system->records[index];

        if (!record->active) {
            continue;
        }

        summary->active_fault_count++;
        switch (record->severity) {
        case FAULT_SEVERITY_WARNING:
            summary->warning_count++;
            break;
        case FAULT_SEVERITY_FAULT:
            summary->fault_count++;
            break;
        case FAULT_SEVERITY_CRITICAL:
            summary->critical_count++;
            break;
        case FAULT_SEVERITY_COUNT:
        default:
            return HEALTH_EVALUATE_INVALID_ARGUMENT;
        }
    }

    if ((fault_system->state_machine->current == SYSTEM_STATE_FAULT) ||
        (summary->critical_count > 0U)) {
        summary->state = HEALTH_STATE_CRITICAL;
    } else if (!summary->fault_data_complete) {
        summary->state = HEALTH_STATE_UNKNOWN;
    } else if (summary->fault_count > 0U) {
        summary->state = HEALTH_STATE_DEGRADED;
    } else if (summary->warning_count > 0U) {
        summary->state = HEALTH_STATE_WARNING;
    }

    return HEALTH_EVALUATE_OK;
}

const char *health_state_name(health_state_t state)
{
    switch (state) {
    case HEALTH_STATE_OK:
        return "OK";
    case HEALTH_STATE_WARNING:
        return "WARNING";
    case HEALTH_STATE_DEGRADED:
        return "DEGRADED";
    case HEALTH_STATE_UNKNOWN:
        return "UNKNOWN";
    case HEALTH_STATE_CRITICAL:
        return "CRITICAL";
    case HEALTH_STATE_COUNT:
        break;
    }

    return "UNKNOWN";
}
