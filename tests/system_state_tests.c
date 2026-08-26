#include "system_state.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

typedef struct {
    bool accepted;
    system_state_t next;
} expected_transition_t;

static const expected_transition_t transition_table[SYSTEM_STATE_COUNT]
                                                    [SYSTEM_STATE_EVENT_COUNT] = {
    [SYSTEM_STATE_BOOT] = {
        [SYSTEM_STATE_EVENT_INITIALIZATION_STARTED] =
            {true, SYSTEM_STATE_INITIALIZING},
        [SYSTEM_STATE_EVENT_FAULT_DETECTED] = {true, SYSTEM_STATE_FAULT},
    },
    [SYSTEM_STATE_INITIALIZING] = {
        [SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED] =
            {true, SYSTEM_STATE_DISARMED},
        [SYSTEM_STATE_EVENT_FAULT_DETECTED] = {true, SYSTEM_STATE_FAULT},
    },
    [SYSTEM_STATE_DISARMED] = {
        [SYSTEM_STATE_EVENT_ARM_REQUESTED] = {true, SYSTEM_STATE_ARMED},
        [SYSTEM_STATE_EVENT_FAULT_DETECTED] = {true, SYSTEM_STATE_FAULT},
    },
    [SYSTEM_STATE_ARMED] = {
        [SYSTEM_STATE_EVENT_DISARM_REQUESTED] = {true, SYSTEM_STATE_DISARMED},
        [SYSTEM_STATE_EVENT_FAILSAFE_DETECTED] = {true, SYSTEM_STATE_FAILSAFE},
        [SYSTEM_STATE_EVENT_FAULT_DETECTED] = {true, SYSTEM_STATE_FAULT},
    },
    [SYSTEM_STATE_FAILSAFE] = {
        [SYSTEM_STATE_EVENT_DISARM_REQUESTED] = {true, SYSTEM_STATE_DISARMED},
        [SYSTEM_STATE_EVENT_FAULT_DETECTED] = {true, SYSTEM_STATE_FAULT},
    },
};

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

static void initializes_in_boot(void)
{
    system_state_machine_t state_machine = {
        .current = SYSTEM_STATE_FAULT,
        .previous = SYSTEM_STATE_ARMED,
        .transition_count = 4U,
        .rejected_transition_count = 5U,
    };

    system_state_machine_initialize(NULL);
    system_state_machine_initialize(&state_machine);

    assert(state_machine.current == SYSTEM_STATE_BOOT);
    assert(state_machine.previous == SYSTEM_STATE_BOOT);
    assert(state_machine.transition_count == 0U);
    assert(state_machine.rejected_transition_count == 0U);
    assert(state_machine.initialized);
}

static void verifies_every_state_event_pair(void)
{
    system_state_t state;
    system_state_event_t event;

    for (state = SYSTEM_STATE_BOOT; state < SYSTEM_STATE_COUNT; state++) {
        for (event = SYSTEM_STATE_EVENT_INITIALIZATION_STARTED;
             event < SYSTEM_STATE_EVENT_COUNT;
             event++) {
            const expected_transition_t expected =
                transition_table[state][event];
            system_state_machine_t state_machine;
            system_state_t previous;
            uint32_t transition_count;
            uint32_t rejected_count;

            prepare_state(&state_machine, state);
            previous = state_machine.previous;
            transition_count = state_machine.transition_count;
            rejected_count = state_machine.rejected_transition_count;

            if (expected.accepted) {
                assert(system_state_machine_handle_event(&state_machine,
                                                         event) ==
                       SYSTEM_STATE_TRANSITION_OK);
                assert(state_machine.current == expected.next);
                assert(state_machine.previous == state);
                assert(state_machine.transition_count ==
                       transition_count + 1U);
                assert(state_machine.rejected_transition_count ==
                       rejected_count);
            } else {
                assert(system_state_machine_handle_event(&state_machine,
                                                         event) ==
                       SYSTEM_STATE_TRANSITION_REJECTED);
                assert(state_machine.current == state);
                assert(state_machine.previous == previous);
                assert(state_machine.transition_count == transition_count);
                assert(state_machine.rejected_transition_count ==
                       rejected_count + 1U);
            }
        }
    }
}

static void rejects_invalid_arguments_without_changing_state(void)
{
    system_state_machine_t uninitialized = {0};
    system_state_machine_t state_machine;

    system_state_machine_initialize(&state_machine);

    assert(system_state_machine_handle_event(
               NULL,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);
    assert(system_state_machine_handle_event(
               &uninitialized,
               SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_COUNT) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);
    assert(system_state_machine_handle_event(
               &state_machine,
               (system_state_event_t)-1) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);
    assert(state_machine.current == SYSTEM_STATE_BOOT);
    assert(state_machine.transition_count == 0U);
    assert(state_machine.rejected_transition_count == 0U);

    state_machine.current = SYSTEM_STATE_COUNT;
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_FAULT_DETECTED) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);

    system_state_machine_initialize(&state_machine);
    state_machine.current = (system_state_t)-1;
    assert(system_state_machine_handle_event(
               &state_machine,
               SYSTEM_STATE_EVENT_FAULT_DETECTED) ==
           SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT);
}

static void counters_saturate(void)
{
    system_state_machine_t accepted;
    system_state_machine_t rejected;

    prepare_state(&accepted, SYSTEM_STATE_DISARMED);
    accepted.transition_count = UINT32_MAX;
    assert(system_state_machine_handle_event(
               &accepted,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_OK);
    assert(accepted.transition_count == UINT32_MAX);

    prepare_state(&rejected, SYSTEM_STATE_BOOT);
    rejected.rejected_transition_count = UINT32_MAX;
    assert(system_state_machine_handle_event(
               &rejected,
               SYSTEM_STATE_EVENT_ARM_REQUESTED) ==
           SYSTEM_STATE_TRANSITION_REJECTED);
    assert(rejected.rejected_transition_count == UINT32_MAX);
}

int main(void)
{
    initializes_in_boot();
    verifies_every_state_event_pair();
    rejects_invalid_arguments_without_changing_state();
    counters_saturate();
    return 0;
}
