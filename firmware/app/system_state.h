#ifndef OPENFLIGHTCOMPUTER_SYSTEM_STATE_H
#define OPENFLIGHTCOMPUTER_SYSTEM_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SYSTEM_STATE_BOOT = 0,
    SYSTEM_STATE_INITIALIZING,
    SYSTEM_STATE_DISARMED,
    SYSTEM_STATE_ARMED,
    SYSTEM_STATE_FAILSAFE,
    SYSTEM_STATE_FAULT,
    SYSTEM_STATE_COUNT,
} system_state_t;

typedef enum {
    SYSTEM_STATE_EVENT_INITIALIZATION_STARTED = 0,
    SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED,
    SYSTEM_STATE_EVENT_ARM_REQUESTED,
    SYSTEM_STATE_EVENT_DISARM_REQUESTED,
    SYSTEM_STATE_EVENT_FAILSAFE_DETECTED,
    SYSTEM_STATE_EVENT_FAULT_DETECTED,
    SYSTEM_STATE_EVENT_COUNT,
} system_state_event_t;

typedef enum {
    SYSTEM_STATE_TRANSITION_OK = 0,
    SYSTEM_STATE_TRANSITION_REJECTED,
    SYSTEM_STATE_TRANSITION_INVALID_ARGUMENT,
} system_state_transition_result_t;

typedef struct {
    system_state_t current;
    system_state_t previous;
    uint32_t transition_count;
    uint32_t rejected_transition_count;
    bool initialized;
} system_state_machine_t;

void system_state_machine_initialize(system_state_machine_t *state_machine);
system_state_transition_result_t system_state_machine_handle_event(
    system_state_machine_t *state_machine,
    system_state_event_t event);

#endif
