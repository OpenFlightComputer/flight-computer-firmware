#ifndef OPENFLIGHTCOMPUTER_MOTOR_CONTROL_INTERNAL_H
#define OPENFLIGHTCOMPUTER_MOTOR_CONTROL_INTERNAL_H

#include "motor_control.h"
#include "fault.h"
#include "motor_output.h"
#include "system_state.h"

#include <stdint.h>

typedef uint64_t (*motor_control_clock_t)(void);

typedef enum {
    MOTOR_CONTROL_INIT_OK = 0,
    MOTOR_CONTROL_INIT_INVALID_ARGUMENT,
    MOTOR_CONTROL_INIT_ALREADY_INITIALIZED,
    MOTOR_CONTROL_INIT_BACKEND_ERROR,
    MOTOR_CONTROL_INIT_INITIAL_STOP_ERROR,
} motor_control_init_result_t;

/* Internal application composition API; not a general command-source API. */
motor_control_init_result_t motor_control_initialize(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    motor_control_clock_t clock,
    uint64_t command_timeout_us,
    const motor_output_backend_t *backend);

#endif
