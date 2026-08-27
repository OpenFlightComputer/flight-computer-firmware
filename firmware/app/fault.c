#include "fault.h"

#include <limits.h>
#include <stddef.h>

typedef struct {
    uint64_t timestamp_us;
    bool valid;
} fault_time_t;

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static bool severity_is_valid(fault_severity_t severity)
{
    switch (severity) {
    case FAULT_SEVERITY_WARNING:
    case FAULT_SEVERITY_FAULT:
    case FAULT_SEVERITY_CRITICAL:
        return true;
    case FAULT_SEVERITY_COUNT:
        break;
    }

    return false;
}

static bool source_is_valid(fault_source_t source)
{
    switch (source) {
    case FAULT_SOURCE_APPLICATION:
    case FAULT_SOURCE_BOARD:
    case FAULT_SOURCE_MCU:
    case FAULT_SOURCE_SCHEDULER:
    case FAULT_SOURCE_STATE_MACHINE:
    case FAULT_SOURCE_USB:
        return true;
    case FAULT_SOURCE_COUNT:
        break;
    }

    return false;
}

static bool catalog_is_valid(const fault_definition_t *definitions,
                             size_t definition_count)
{
    size_t index;
    size_t comparison_index;

    if ((definitions == NULL) || (definition_count == 0U) ||
        (definition_count > FAULT_DEFINITION_CAPACITY)) {
        return false;
    }

    for (index = 0U; index < definition_count; index++) {
        const fault_definition_t *definition = &definitions[index];

        if ((definition->id == FAULT_ID_INVALID) ||
            !severity_is_valid(definition->severity) ||
            !source_is_valid(definition->source)) {
            return false;
        }

        for (comparison_index = index + 1U;
             comparison_index < definition_count;
             comparison_index++) {
            if (definition->id == definitions[comparison_index].id) {
                return false;
            }
        }
    }

    return true;
}

static const fault_definition_t *definition_for_id(
    const fault_system_t *system,
    fault_id_t id)
{
    size_t index;

    for (index = 0U; index < system->definition_count; index++) {
        if (system->definitions[index].id == id) {
            return &system->definitions[index];
        }
    }

    return NULL;
}

static fault_record_t *record_for_id(fault_system_t *system, fault_id_t id)
{
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        fault_record_t *record = &system->records[index];

        if (record->active && (record->id == id)) {
            return record;
        }
    }

    return NULL;
}

static fault_record_t *first_free_record(fault_system_t *system)
{
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        if (!system->records[index].active) {
            return &system->records[index];
        }
    }

    return NULL;
}

static fault_time_t current_fault_time(const fault_system_t *system)
{
    if (system->clock == NULL) {
        return (fault_time_t){0};
    }

    return (fault_time_t){
        .timestamp_us = system->clock(),
        .valid = true,
    };
}

static void initialize_record(fault_record_t *record,
                              const fault_definition_t *definition,
                              fault_time_t time,
                              bool context_valid,
                              uint32_t context)
{
    *record = (fault_record_t){
        .id = definition->id,
        .severity = definition->severity,
        .source = definition->source,
        .first_timestamp_us = time.timestamp_us,
        .last_timestamp_us = time.timestamp_us,
        .context = context,
        .occurrence_count = 1U,
        .first_timestamp_valid = time.valid,
        .last_timestamp_valid = time.valid,
        .context_valid = context_valid,
        .active = true,
    };
}

static void update_record(fault_record_t *record,
                          fault_time_t time,
                          bool context_valid,
                          uint32_t context)
{
    record->last_timestamp_us = time.timestamp_us;
    record->last_timestamp_valid = time.valid;
    record->context = context;
    record->context_valid = context_valid;
    saturating_increment(&record->occurrence_count);
}

static bool apply_critical_state_transition(
    fault_system_t *system,
    const fault_definition_t *definition)
{
    if ((definition->severity != FAULT_SEVERITY_CRITICAL) ||
        (system->state_machine->current == SYSTEM_STATE_FAULT)) {
        return true;
    }

    if (system_state_machine_handle_event(
            system->state_machine,
            SYSTEM_STATE_EVENT_FAULT_DETECTED) !=
        SYSTEM_STATE_TRANSITION_OK) {
        saturating_increment(&system->critical_transition_failure_count);
        return false;
    }

    return true;
}

fault_init_result_t fault_system_initialize(
    fault_system_t *system,
    system_state_machine_t *state_machine,
    const fault_definition_t *definitions,
    size_t definition_count)
{
    if (system == NULL) {
        return FAULT_INIT_INVALID_ARGUMENT;
    }

    *system = (fault_system_t){0};

    if ((state_machine == NULL) || !state_machine->initialized) {
        return FAULT_INIT_INVALID_ARGUMENT;
    }
    if (!catalog_is_valid(definitions, definition_count)) {
        return FAULT_INIT_INVALID_CATALOG;
    }

    system->state_machine = state_machine;
    system->definitions = definitions;
    system->definition_count = definition_count;
    system->initialized = true;

    return FAULT_INIT_OK;
}

fault_clock_attach_result_t fault_system_attach_clock(fault_system_t *system,
                                                      fault_clock_t clock)
{
    if ((system == NULL) || !system->initialized || (clock == NULL)) {
        return FAULT_CLOCK_ATTACH_INVALID_ARGUMENT;
    }

    system->clock = clock;
    return FAULT_CLOCK_ATTACH_OK;
}

fault_report_result_t fault_system_report(fault_system_t *system,
                                          fault_id_t id,
                                          bool context_valid,
                                          uint32_t context)
{
    const fault_definition_t *definition;
    fault_report_result_t result;
    fault_record_t *record;
    fault_time_t time;

    if ((system == NULL) || !system->initialized ||
        (system->state_machine == NULL)) {
        return FAULT_REPORT_INVALID_ARGUMENT;
    }

    definition = definition_for_id(system, id);
    if (definition == NULL) {
        return FAULT_REPORT_UNKNOWN_ID;
    }

    time = current_fault_time(system);
    record = record_for_id(system, id);
    if (record != NULL) {
        update_record(record, time, context_valid, context);
        result = FAULT_REPORT_UPDATED;
    } else {
        record = first_free_record(system);
        if (record == NULL) {
            saturating_increment(&system->dropped_record_count);
            result = FAULT_REPORT_CAPACITY_EXCEEDED;
        } else {
            initialize_record(record,
                              definition,
                              time,
                              context_valid,
                              context);
            system->active_count++;
            result = FAULT_REPORT_RECORDED;
        }
    }

    if (!apply_critical_state_transition(system, definition)) {
        return FAULT_REPORT_CRITICAL_TRANSITION_FAILED;
    }

    return result;
}

fault_clear_result_t fault_system_clear(fault_system_t *system, fault_id_t id)
{
    const fault_definition_t *definition;
    fault_record_t *record;

    if ((system == NULL) || !system->initialized) {
        return FAULT_CLEAR_INVALID_ARGUMENT;
    }

    definition = definition_for_id(system, id);
    if (definition == NULL) {
        return FAULT_CLEAR_UNKNOWN_ID;
    }
    record = record_for_id(system, id);
    if (record == NULL) {
        return FAULT_CLEAR_NOT_ACTIVE;
    }
    if (definition->severity == FAULT_SEVERITY_CRITICAL) {
        return FAULT_CLEAR_CRITICAL_LATCHED;
    }

    *record = (fault_record_t){0};
    system->active_count--;
    return FAULT_CLEAR_OK;
}

const fault_record_t *fault_system_record_for_id(const fault_system_t *system,
                                                fault_id_t id)
{
    size_t index;

    if ((system == NULL) || !system->initialized) {
        return NULL;
    }

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        const fault_record_t *record = &system->records[index];

        if (record->active && (record->id == id)) {
            return record;
        }
    }

    return NULL;
}

size_t fault_system_active_count(const fault_system_t *system)
{
    if ((system == NULL) || !system->initialized) {
        return 0U;
    }

    return system->active_count;
}
