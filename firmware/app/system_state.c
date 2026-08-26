#include "system_state.h"

#include <limits.h>
#include <stddef.h>

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static bool state_is_valid(system_state_t state)
{
    switch (state) {
    case SYSTEM_STATE_BOOT:
    case SYSTEM_STATE_INITIALIZING:
    case SYSTEM_STATE_DISARMED:
    case SYSTEM_STATE_ARMED:
    case SYSTEM_STATE_FAILSAFE:
    case SYSTEM_STATE_FAULT:
        return true;
    case SYSTEM_STATE_COUNT:
        break;
    }

    return false;
}

static bool event_is_valid(system_state_event_t event)
{
    switch (event) {
    case SYSTEM_STATE_EVENT_INITIALIZATION_STARTED:
    case SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED:
    case SYSTEM_STATE_EVENT_ARM_REQUESTED:
    case SYSTEM_STATE_EVENT_DISARM_REQUESTED:
    case SYSTEM_STATE_EVENT_FAILSAFE_DETECTED:
    case SYSTEM_STATE_EVENT_FAULT_DETECTED:
        return true;
    case SYSTEM_STATE_EVENT_COUNT:
        break;
    }

    return false;
}

static bool next_state_for_event(system_state_t current,
                                 system_state_event_t event,
                                 system_state_t *next)
{
    if (event == SYSTEM_STATE_EVENT_FAULT_DETECTED) {
        if (current != SYSTEM_STATE_FAULT) {
            *next = SYSTEM_STATE_FAULT;
            return true;
        }
        return false;
    }

    switch (current) {
    case SYSTEM_STATE_BOOT:
        if (event == SYSTEM_STATE_EVENT_INITIALIZATION_STARTED) {
            *next = SYSTEM_STATE_INITIALIZING;
            return true;
        }
        break;
    case SYSTEM_STATE_INITIALIZING:
        if (event == SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED) {
            *next = SYSTEM_STATE_DISARMED;
            return true;
        }
        break;
    case SYSTEM_STATE_DISARMED:
        if (event == SYSTEM_STATE_EVENT_ARM_REQUESTED) {
            *next = SYSTEM_STATE_ARMED;
            return true;
        }
        break;
    case SYSTEM_STATE_ARMED:
        if (event == SYSTEM_STATE_EVENT_DISARM_REQUESTED) {
            *next = SYSTEM_STATE_DISARMED;
            return true;
        }
        if (event == SYSTEM_STATE_EVENT_FAILSAFE_DETECTED) {
            *next = SYSTEM_STATE_FAILSAFE;
            return true;
        }
        break;
    case SYSTEM_STATE_FAILSAFE:
        if (event == SYSTEM_STATE_EVENT_DISARM_REQUESTED) {
            *next = SYSTEM_STATE_DISARMED;
            return true;
        }
        break;
    case SYSTEM_STATE_FAULT:
    case SYSTEM_STATE_COUNT:
        break;
    }

    return false;
}

void system_state_machine_initialize(system_state_machine_t *state_machine)
{
    if (state_machine != NULL) {
        *state_machine = (system_state_machine_t){
            .current = SYSTEM_STATE_BOOT,
            .previous = SYSTEM_STATE_BOOT,
            .initialized = true,
        };
    }
}

system_state_transition_result_t system_state_machine_handle_event(
    system_state_machine_t *state_machine,
    system_state_event_t event)
{
    system_state_t next;

    if ((state_machine == NULL) || !state_machine->initialized ||
        !state_is_valid(state_machine->current) || !event_is_valid(event)) {
        return SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT;
    }

    if (!next_state_for_event(state_machine->current, event, &next)) {
        saturating_increment(&state_machine->rejected_transition_count);
        return SYSTEM_STATE_TRANSITION_REJECTED;
    }

    state_machine->previous = state_machine->current;
    state_machine->current = next;
    saturating_increment(&state_machine->transition_count);

    return SYSTEM_STATE_TRANSITION_OK;
}
