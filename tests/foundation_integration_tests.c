#include "fault.h"
#include "health.h"
#include "system_state.h"

#include <assert.h>
#include <stddef.h>

static const fault_definition_t definitions[] = {
    {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    {2U, FAULT_SEVERITY_FAULT, FAULT_SOURCE_USB},
    {3U, FAULT_SEVERITY_CRITICAL, FAULT_SOURCE_MCU},
};

static void initialize_foundation(system_state_machine_t *state_machine,
                                  fault_system_t *fault_system)
{
    system_state_machine_initialize(state_machine);
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   definitions,
                                   sizeof(definitions) /
                                       sizeof(definitions[0])) ==
           FAULT_INIT_OK);
}

static void complete_startup(system_state_machine_t *state_machine)
{
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED) ==
           SYSTEM_STATE_TRANSITION_OK);
}

static void successful_startup_is_disarmed_and_healthy(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;

    initialize_foundation(&state_machine, &fault_system);
    complete_startup(&state_machine);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_OK);
}

static void noncritical_startup_failure_allows_degraded_disarmed_state(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;

    initialize_foundation(&state_machine, &fault_system);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(fault_system_report(&fault_system, 2U, true, 99U) ==
           FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_INITIALIZING);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_DEGRADED);
}

static void critical_startup_failure_is_terminal_and_visible(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;

    initialize_foundation(&state_machine, &fault_system);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(fault_system_report(&fault_system, 3U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED) ==
           SYSTEM_STATE_TRANSITION_REJECTED);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_CRITICAL);
}

static void recoverable_runtime_fault_does_not_force_disarm(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;

    initialize_foundation(&state_machine, &fault_system);
    complete_startup(&state_machine);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(fault_system_report(&fault_system, 2U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_DEGRADED);
    assert(fault_system_clear(&fault_system, 2U) == FAULT_CLEAR_OK);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_OK);
}

static void critical_runtime_fault_overrides_armed_state(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;

    initialize_foundation(&state_machine, &fault_system);
    complete_startup(&state_machine);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(fault_system_report(&fault_system, 3U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_CRITICAL);
}

static void registry_capacity_failure_is_terminal(void)
{
    fault_definition_t capacity_definitions[FAULT_SYSTEM_CAPACITY + 1U];
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t health;
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY + 1U; index++) {
        capacity_definitions[index] = (fault_definition_t){
            .id = (fault_id_t)(index + 1U),
            .severity = FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }
    system_state_machine_initialize(&state_machine);
    assert(fault_system_initialize(&fault_system,
                                   &state_machine,
                                   capacity_definitions,
                                   FAULT_SYSTEM_CAPACITY + 1U) ==
           FAULT_INIT_OK);
    complete_startup(&state_machine);
    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        assert(fault_system_report(&fault_system,
                                   capacity_definitions[index].id,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
    }
    assert(fault_system_report(
               &fault_system,
               capacity_definitions[FAULT_SYSTEM_CAPACITY].id,
               false,
               0U) == FAULT_REPORT_CAPACITY_EXCEEDED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
    assert(fault_system.dropped_record_count == 1U);
    assert(health_evaluate(&fault_system, &health) == HEALTH_EVALUATE_OK);
    assert(health.state == HEALTH_STATE_CRITICAL);
    assert(!health.fault_data_complete);
}

int main(void)
{
    successful_startup_is_disarmed_and_healthy();
    noncritical_startup_failure_allows_degraded_disarmed_state();
    critical_startup_failure_is_terminal_and_visible();
    recoverable_runtime_fault_does_not_force_disarm();
    critical_runtime_fault_overrides_armed_state();
    registry_capacity_failure_is_terminal();
    return 0;
}
