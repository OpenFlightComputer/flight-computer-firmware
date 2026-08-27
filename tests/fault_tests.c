#include "fault.h"
#include "fault_catalog.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define TEST_WARNING_ID UINT16_C(100)
#define TEST_FAULT_ID UINT16_C(101)
#define TEST_CRITICAL_ID UINT16_C(102)

static const fault_definition_t test_definitions[] = {
    {
        .id = TEST_WARNING_ID,
        .severity = FAULT_SEVERITY_WARNING,
        .source = FAULT_SOURCE_APPLICATION,
    },
    {
        .id = TEST_FAULT_ID,
        .severity = FAULT_SEVERITY_FAULT,
        .source = FAULT_SOURCE_BOARD,
    },
    {
        .id = TEST_CRITICAL_ID,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_MCU,
    },
};

static uint64_t fake_time_us;

static uint64_t fake_clock(void)
{
    return fake_time_us;
}

static void prepare_state(system_state_machine_t *state_machine,
                          system_state_t target)
{
    system_state_machine_initialize(state_machine);

    if (target == SYSTEM_STATE_BOOT) {
        return;
    }
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    if (target == SYSTEM_STATE_INITIALIZING) {
        return;
    }
    if (target == SYSTEM_STATE_FAULT) {
        assert(system_state_machine_handle_event(
                   state_machine,
                   SYSTEM_STATE_EVENT_FAULT_DETECTED) ==
               SYSTEM_STATE_TRANSITION_OK);
        return;
    }
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED) ==
           SYSTEM_STATE_TRANSITION_OK);
    if (target == SYSTEM_STATE_DISARMED) {
        return;
    }
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    if (target == SYSTEM_STATE_ARMED) {
        return;
    }
    assert(target == SYSTEM_STATE_FAILSAFE);
    assert(system_state_machine_handle_event(
               state_machine,
               SYSTEM_STATE_EVENT_FAILSAFE_DETECTED) ==
           SYSTEM_STATE_TRANSITION_OK);
}

static void initialize_test_system(fault_system_t *system,
                                   system_state_machine_t *state_machine,
                                   system_state_t target)
{
    prepare_state(state_machine, target);
    assert(fault_system_initialize(
               system,
               state_machine,
               test_definitions,
               sizeof(test_definitions) / sizeof(test_definitions[0])) ==
           FAULT_INIT_OK);
}

static void validates_initialization_and_catalogs(void)
{
    fault_definition_t invalid_definition = test_definitions[0];
    fault_definition_t duplicate_definitions[2] = {
        test_definitions[0],
        test_definitions[0],
    };
    fault_definition_t too_many[FAULT_DEFINITION_CAPACITY + 1U];
    system_state_machine_t state_machine;
    system_state_machine_t uninitialized_state = {0};
    fault_system_t system;
    const fault_definition_t *firmware_definitions;
    size_t firmware_definition_count;
    size_t index;

    prepare_state(&state_machine, SYSTEM_STATE_BOOT);
    assert(fault_system_initialize(NULL,
                                   &state_machine,
                                   test_definitions,
                                   3U) == FAULT_INIT_INVALID_ARGUMENT);
    assert(fault_system_initialize(&system,
                                   NULL,
                                   test_definitions,
                                   3U) == FAULT_INIT_INVALID_ARGUMENT);
    assert(fault_system_initialize(&system,
                                   &uninitialized_state,
                                   test_definitions,
                                   3U) == FAULT_INIT_INVALID_ARGUMENT);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   NULL,
                                   3U) == FAULT_INIT_INVALID_CATALOG);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   test_definitions,
                                   0U) == FAULT_INIT_INVALID_CATALOG);

    for (index = 0U; index < FAULT_DEFINITION_CAPACITY + 1U; index++) {
        too_many[index] = (fault_definition_t){
            .id = (fault_id_t)(index + 1U),
            .severity = FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }
    assert(fault_system_initialize(
               &system,
               &state_machine,
               too_many,
               FAULT_DEFINITION_CAPACITY + 1U) ==
           FAULT_INIT_INVALID_CATALOG);

    invalid_definition.id = FAULT_ID_INVALID;
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   &invalid_definition,
                                   1U) == FAULT_INIT_INVALID_CATALOG);
    invalid_definition = test_definitions[0];
    invalid_definition.severity = (fault_severity_t)-1;
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   &invalid_definition,
                                   1U) == FAULT_INIT_INVALID_CATALOG);
    invalid_definition = test_definitions[0];
    invalid_definition.source = FAULT_SOURCE_COUNT;
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   &invalid_definition,
                                   1U) == FAULT_INIT_INVALID_CATALOG);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   duplicate_definitions,
                                   2U) == FAULT_INIT_INVALID_CATALOG);

    firmware_definitions =
        firmware_fault_catalog(&firmware_definition_count);
    assert(firmware_definition_count == 10U);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   firmware_definitions,
                                   firmware_definition_count) ==
           FAULT_INIT_OK);
    assert(system.initialized);
    assert(system.state_machine == &state_machine);
    assert(system.active_count == 0U);
}

static void severity_controls_state_and_preserves_diagnostics(void)
{
    system_state_machine_t state_machine;
    fault_system_t system;
    const fault_record_t *record;

    initialize_test_system(&system,
                           &state_machine,
                           SYSTEM_STATE_INITIALIZING);

    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               true,
                               UINT32_C(11)) == FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_INITIALIZING);

    assert(fault_system_report(&system,
                               TEST_FAULT_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_INITIALIZING);

    assert(fault_system_report(&system,
                               TEST_CRITICAL_ID,
                               true,
                               UINT32_C(33)) == FAULT_REPORT_RECORDED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
    assert(state_machine.previous == SYSTEM_STATE_INITIALIZING);
    assert(fault_system_active_count(&system) == 3U);

    record = fault_system_record_for_id(&system, TEST_CRITICAL_ID);
    assert(record != NULL);
    assert(record->severity == FAULT_SEVERITY_CRITICAL);
    assert(record->source == FAULT_SOURCE_MCU);
    assert(record->context_valid);
    assert(record->context == 33U);
    assert(record->occurrence_count == 1U);
    assert(!record->first_timestamp_valid);
    assert(!record->last_timestamp_valid);

    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_REJECTED);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
}

static void critical_fault_transitions_every_non_fault_state(void)
{
    system_state_t target;

    for (target = SYSTEM_STATE_BOOT; target < SYSTEM_STATE_COUNT; target++) {
        system_state_machine_t state_machine;
        fault_system_t system;
        uint32_t transition_count;

        initialize_test_system(&system, &state_machine, target);
        transition_count = state_machine.transition_count;

        assert(fault_system_report(&system,
                                   TEST_CRITICAL_ID,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
        assert(state_machine.current == SYSTEM_STATE_FAULT);
        if (target == SYSTEM_STATE_FAULT) {
            assert(state_machine.transition_count == transition_count);
        } else {
            assert(state_machine.previous == target);
            assert(state_machine.transition_count == transition_count + 1U);
        }
        assert(system.critical_transition_failure_count == 0U);
    }
}

static void tracks_first_and_latest_occurrences(void)
{
    system_state_machine_t state_machine;
    fault_system_t system;
    const fault_record_t *record;

    initialize_test_system(&system, &state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               true,
                               UINT32_C(10)) == FAULT_REPORT_RECORDED);

    assert(fault_system_attach_clock(&system, fake_clock) ==
           FAULT_CLOCK_ATTACH_OK);
    fake_time_us = 100U;
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_UPDATED);
    fake_time_us = 250U;
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               true,
                               UINT32_C(25)) == FAULT_REPORT_UPDATED);

    record = fault_system_record_for_id(&system, TEST_WARNING_ID);
    assert(record != NULL);
    assert(record->occurrence_count == 3U);
    assert(record->first_timestamp_us == 0U);
    assert(!record->first_timestamp_valid);
    assert(record->last_timestamp_us == 250U);
    assert(record->last_timestamp_valid);
    assert(record->context_valid);
    assert(record->context == 25U);
    assert(fault_system_active_count(&system) == 1U);
}

static void clears_recoverable_records_and_latches_critical_records(void)
{
    system_state_machine_t state_machine;
    fault_system_t system;

    initialize_test_system(&system, &state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_clear(&system, TEST_WARNING_ID) ==
           FAULT_CLEAR_NOT_ACTIVE);
    assert(fault_system_clear(&system, TEST_CRITICAL_ID) ==
           FAULT_CLEAR_NOT_ACTIVE);
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(fault_system_report(&system,
                               TEST_FAULT_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(fault_system_clear(&system, TEST_WARNING_ID) == FAULT_CLEAR_OK);
    assert(fault_system_clear(&system, TEST_FAULT_ID) == FAULT_CLEAR_OK);
    assert(fault_system_active_count(&system) == 0U);

    assert(fault_system_report(&system,
                               TEST_CRITICAL_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(fault_system_clear(&system, TEST_CRITICAL_ID) ==
           FAULT_CLEAR_CRITICAL_LATCHED);
    assert(fault_system_record_for_id(&system, TEST_CRITICAL_ID) != NULL);
    assert(fault_system_active_count(&system) == 1U);
}

static void capacity_exhaustion_still_applies_critical_state(void)
{
    fault_definition_t definitions[FAULT_SYSTEM_CAPACITY + 1U];
    system_state_machine_t state_machine;
    fault_system_t system;
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY + 1U; index++) {
        definitions[index] = (fault_definition_t){
            .id = (fault_id_t)(UINT16_C(1000) + index),
            .severity = (index == FAULT_SYSTEM_CAPACITY)
                            ? FAULT_SEVERITY_CRITICAL
                            : FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }

    prepare_state(&state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   definitions,
                                   FAULT_SYSTEM_CAPACITY + 1U) ==
           FAULT_INIT_OK);

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        assert(fault_system_report(&system,
                                   definitions[index].id,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
    }
    assert(fault_system_report(&system,
                               definitions[FAULT_SYSTEM_CAPACITY].id,
                               false,
                               0U) == FAULT_REPORT_CAPACITY_EXCEEDED);
    assert(system.dropped_record_count == 1U);
    assert(fault_system_active_count(&system) == FAULT_SYSTEM_CAPACITY);
    assert(state_machine.current == SYSTEM_STATE_FAULT);
}

static void cleared_slots_are_reused(void)
{
    fault_definition_t definitions[FAULT_SYSTEM_CAPACITY + 1U];
    system_state_machine_t state_machine;
    fault_system_t system;
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY + 1U; index++) {
        definitions[index] = (fault_definition_t){
            .id = (fault_id_t)(UINT16_C(2000) + index),
            .severity = FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }

    prepare_state(&state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   definitions,
                                   FAULT_SYSTEM_CAPACITY + 1U) ==
           FAULT_INIT_OK);
    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        assert(fault_system_report(&system,
                                   definitions[index].id,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
    }

    assert(fault_system_clear(&system, definitions[3].id) == FAULT_CLEAR_OK);
    assert(fault_system_report(&system,
                               definitions[FAULT_SYSTEM_CAPACITY].id,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(fault_system_active_count(&system) == FAULT_SYSTEM_CAPACITY);
    assert(fault_system_record_for_id(
               &system,
               definitions[FAULT_SYSTEM_CAPACITY].id) != NULL);
}

static void rejects_invalid_operations(void)
{
    system_state_machine_t state_machine;
    fault_system_t system;
    fault_system_t uninitialized = {0};

    initialize_test_system(&system, &state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_attach_clock(NULL, fake_clock) ==
           FAULT_CLOCK_ATTACH_INVALID_ARGUMENT);
    assert(fault_system_attach_clock(&uninitialized, fake_clock) ==
           FAULT_CLOCK_ATTACH_INVALID_ARGUMENT);
    assert(fault_system_attach_clock(&system, NULL) ==
           FAULT_CLOCK_ATTACH_INVALID_ARGUMENT);
    assert(fault_system_report(NULL,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_INVALID_ARGUMENT);
    assert(fault_system_report(&uninitialized,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_INVALID_ARGUMENT);
    assert(fault_system_report(&system,
                               UINT16_C(999),
                               false,
                               0U) == FAULT_REPORT_UNKNOWN_ID);
    assert(fault_system_clear(NULL, TEST_WARNING_ID) ==
           FAULT_CLEAR_INVALID_ARGUMENT);
    assert(fault_system_clear(&uninitialized, TEST_WARNING_ID) ==
           FAULT_CLEAR_INVALID_ARGUMENT);
    assert(fault_system_clear(&system, UINT16_C(999)) ==
           FAULT_CLEAR_UNKNOWN_ID);
    assert(fault_system_record_for_id(NULL, TEST_WARNING_ID) == NULL);
    assert(fault_system_record_for_id(&uninitialized, TEST_WARNING_ID) ==
           NULL);
    assert(fault_system_active_count(NULL) == 0U);
    assert(fault_system_active_count(&uninitialized) == 0U);
}

static void counters_saturate_and_transition_failures_preserve_records(void)
{
    fault_definition_t capacity_definitions[FAULT_SYSTEM_CAPACITY + 1U];
    system_state_machine_t state_machine;
    fault_system_t system;
    fault_record_t *record;
    size_t index;

    initialize_test_system(&system, &state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    record = &system.records[0];
    assert(record->id == TEST_WARNING_ID);
    record->occurrence_count = UINT32_MAX;
    assert(fault_system_report(&system,
                               TEST_WARNING_ID,
                               false,
                               0U) == FAULT_REPORT_UPDATED);
    assert(record->occurrence_count == UINT32_MAX);

    state_machine.current = SYSTEM_STATE_COUNT;
    system.critical_transition_failure_count = UINT32_MAX;
    assert(fault_system_report(&system,
                               TEST_CRITICAL_ID,
                               true,
                               UINT32_C(77)) ==
           FAULT_REPORT_CRITICAL_TRANSITION_FAILED);
    assert(system.critical_transition_failure_count == UINT32_MAX);
    assert(fault_system_record_for_id(&system, TEST_CRITICAL_ID) != NULL);

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY + 1U; index++) {
        capacity_definitions[index] = (fault_definition_t){
            .id = (fault_id_t)(UINT16_C(3000) + index),
            .severity = FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }
    prepare_state(&state_machine, SYSTEM_STATE_DISARMED);
    assert(fault_system_initialize(&system,
                                   &state_machine,
                                   capacity_definitions,
                                   FAULT_SYSTEM_CAPACITY + 1U) ==
           FAULT_INIT_OK);
    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        assert(fault_system_report(&system,
                                   capacity_definitions[index].id,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
    }
    system.dropped_record_count = UINT32_MAX;
    assert(fault_system_report(
               &system,
               capacity_definitions[FAULT_SYSTEM_CAPACITY].id,
               false,
               0U) == FAULT_REPORT_CAPACITY_EXCEEDED);
    assert(system.dropped_record_count == UINT32_MAX);
}

int main(void)
{
    validates_initialization_and_catalogs();
    severity_controls_state_and_preserves_diagnostics();
    critical_fault_transitions_every_non_fault_state();
    tracks_first_and_latest_occurrences();
    clears_recoverable_records_and_latches_critical_records();
    capacity_exhaustion_still_applies_critical_state();
    cleared_slots_are_reused();
    rejects_invalid_operations();
    counters_saturate_and_transition_failures_preserve_records();
    return 0;
}
