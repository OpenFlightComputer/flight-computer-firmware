#include "fault.h"
#include "health.h"
#include "system_state.h"

#include <assert.h>
#include <string.h>

static const fault_definition_t severity_definitions[] = {
    {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {2U, FAULT_SEVERITY_FAULT, FAULT_SOURCE_USB},
    {3U, FAULT_SEVERITY_CRITICAL, FAULT_SOURCE_MCU},
};

static const fault_definition_t capacity_definitions[] = {
    {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {2U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {3U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {4U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {5U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {6U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {7U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {8U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {9U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {10U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {11U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {12U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {13U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {14U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {15U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {16U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {17U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
};

static void initialize_fault_system(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    const fault_definition_t *definitions,
    size_t definition_count)
{
    system_state_machine_initialize(state_machine);
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   definitions,
                                   definition_count) == FAULT_INIT_OK);
}

static void severity_precedence_and_counts_are_deterministic(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;

    initialize_fault_system(&state_machine,
                            &fault_system,
                            severity_definitions,
                            3U);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_OK);
    assert(summary.active_fault_count == 0U);
    assert(summary.fault_data_complete);

    assert(fault_system_report(&fault_system, 1U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_WARNING);
    assert(summary.warning_count == 1U);

    assert(fault_system_report(&fault_system, 2U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_DEGRADED);
    assert(summary.active_fault_count == 2U);
    assert(summary.warning_count == 1U);
    assert(summary.fault_count == 1U);

    assert(fault_system_report(&fault_system, 3U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_CRITICAL);
    assert(summary.critical_count == 1U);
}

static void clearing_recoverable_faults_improves_health(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;

    initialize_fault_system(&state_machine,
                            &fault_system,
                            severity_definitions,
                            3U);
    assert(fault_system_report(&fault_system, 2U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_DEGRADED);
    assert(fault_system_clear(&fault_system, 2U) == FAULT_CLEAR_OK);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_OK);
}

static void dropped_records_make_noncritical_health_unknown(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    fault_id_t id;

    initialize_fault_system(&state_machine,
                            &fault_system,
                            capacity_definitions,
                            17U);
    for (id = 1U; id <= FAULT_SYSTEM_CAPACITY; id++) {
        assert(fault_system_report(&fault_system, id, false, 0U) ==
               FAULT_REPORT_RECORDED);
    }
    assert(fault_system_report(&fault_system, 17U, false, 0U) ==
           FAULT_REPORT_CAPACITY_EXCEEDED);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_UNKNOWN);
    assert(!summary.fault_data_complete);
    assert(summary.active_fault_count == FAULT_SYSTEM_CAPACITY);
    assert(summary.dropped_fault_count == 1U);

    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_FAULT_DETECTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_CRITICAL);
}

static void invalid_inputs_and_names_are_safe(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;

    initialize_fault_system(&state_machine,
                            &fault_system,
                            severity_definitions,
                            3U);
    assert(health_evaluate(NULL, &summary) ==
           HEALTH_EVALUATE_INVALID_ARGUMENT);
    assert(health_evaluate(&fault_system, NULL) ==
           HEALTH_EVALUATE_INVALID_ARGUMENT);
    fault_system.records[0] = (fault_record_t){
        .id = 1U,
        .severity = (fault_severity_t)-1,
        .active = true,
    };
    assert(health_evaluate(&fault_system, &summary) ==
           HEALTH_EVALUATE_INVALID_ARGUMENT);

    assert(strcmp(health_state_name(HEALTH_STATE_OK), "OK") == 0);
    assert(strcmp(health_state_name(HEALTH_STATE_WARNING), "WARNING") == 0);
    assert(strcmp(health_state_name(HEALTH_STATE_DEGRADED), "DEGRADED") == 0);
    assert(strcmp(health_state_name(HEALTH_STATE_UNKNOWN), "UNKNOWN") == 0);
    assert(strcmp(health_state_name(HEALTH_STATE_CRITICAL), "CRITICAL") == 0);
    assert(strcmp(health_state_name(HEALTH_STATE_COUNT), "UNKNOWN") == 0);
}

int main(void)
{
    severity_precedence_and_counts_are_deterministic();
    clearing_recoverable_faults_improves_health();
    dropped_records_make_noncritical_health_unknown();
    invalid_inputs_and_names_are_safe();
    return 0;
}
