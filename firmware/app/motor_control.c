#include "motor_control_internal.h"

#include "motor_safety_policy.h"
#include "fault_catalog.h"
#include "motor_mapping.h"

#include <stddef.h>

typedef struct {
    system_state_machine_t *state_machine;
    fault_system_t *fault_system;
    motor_control_clock_t clock;
    uint64_t command_timeout_us;
    motor_output_t physical_output;
    motor_mapping_t mapping;
    motor_command_t last_accepted_command;
    bool outputs_stopped;
    bool initialized;
} motor_control_state_t;

static motor_control_state_t control;

static bool fault_is_declared_critical(const fault_system_t *fault_system,
                                       fault_id_t id)
{
    size_t index;

    for (index = 0U; index < fault_system->definition_count; index++) {
        const fault_definition_t *definition =
            &fault_system->definitions[index];

        if (definition->id == id) {
            return definition->severity == FAULT_SEVERITY_CRITICAL;
        }
    }

    return false;
}

static bool motor_fault_policy_is_valid(
    const fault_system_t *fault_system)
{
    return fault_is_declared_critical(
               fault_system, FAULT_ID_STATE_MACHINE_TRANSITION) &&
           fault_is_declared_critical(
               fault_system, FAULT_ID_MOTOR_INITIALIZATION) &&
           fault_is_declared_critical(
               fault_system, FAULT_ID_MOTOR_OUTPUT) &&
           fault_is_declared_critical(
               fault_system, FAULT_ID_MOTOR_FORCE_STOP);
}

static void report_motor_fault(fault_id_t id, uint32_t context)
{
    (void)fault_system_report(control.fault_system, id, true, context);
}

static bool enter_failsafe_if_armed(void)
{
    system_state_transition_result_t transition_result;

    if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        return true;
    }

    transition_result = system_state_machine_handle_event(
        control.state_machine,
        SYSTEM_STATE_EVENT_FAILSAFE_DETECTED);
    if (transition_result == SYSTEM_STATE_TRANSITION_OK) {
        return true;
    }

    report_motor_fault(FAULT_ID_STATE_MACHINE_TRANSITION,
                          (uint32_t)transition_result);
    return false;
}

static bool force_stop_internal(void)
{
    const motor_output_stop_result_t result =
        motor_output_force_stop(&control.physical_output);

    if (result != MOTOR_OUTPUT_STOP_ACCEPTED) {
        control.outputs_stopped = false;
        report_motor_fault(FAULT_ID_MOTOR_FORCE_STOP,
                              (uint32_t)result);
        return false;
    }

    control.outputs_stopped = true;
    motor_command_invalidate(&control.last_accepted_command);
    return true;
}

static bool current_health_allows_output(void)
{
    health_summary_t summary;

    return (health_evaluate(control.fault_system, &summary) ==
            HEALTH_EVALUATE_OK) &&
           motor_health_allows_output(summary.state);
}

static motor_control_submit_result_t stop_and_return(
    motor_control_submit_result_t result)
{
    if (control.outputs_stopped || force_stop_internal()) {
        return result;
    }

    return MOTOR_CONTROL_SUBMIT_FORCE_STOP_ERROR;
}

motor_control_init_result_t motor_control_initialize(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    motor_control_clock_t clock,
    uint64_t command_timeout_us,
    const motor_output_backend_t *backend)
{
    motor_output_init_result_t output_result;

    if (control.initialized) {
        return MOTOR_CONTROL_INIT_ALREADY_INITIALIZED;
    }
    if ((state_machine == NULL) || !state_machine->initialized ||
        (fault_system == NULL) || !fault_system->initialized ||
        (fault_system->state_machine != state_machine) || (clock == NULL) ||
        (command_timeout_us == 0U) || (backend == NULL) ||
        !motor_fault_policy_is_valid(fault_system)) {
        return MOTOR_CONTROL_INIT_INVALID_ARGUMENT;
    }

    control = (motor_control_state_t){
        .state_machine = state_machine,
        .fault_system = fault_system,
        .clock = clock,
        .command_timeout_us = command_timeout_us,
    };
    motor_mapping_initialize(&control.mapping);
    motor_command_initialize(&control.last_accepted_command);

    output_result = motor_output_initialize(&control.physical_output, backend);
    if (output_result == MOTOR_OUTPUT_INIT_INITIAL_STOP_ERROR) {
        report_motor_fault(FAULT_ID_MOTOR_FORCE_STOP,
                              (uint32_t)output_result);
        return MOTOR_CONTROL_INIT_INITIAL_STOP_ERROR;
    }
    if (output_result != MOTOR_OUTPUT_INIT_OK) {
        report_motor_fault(FAULT_ID_MOTOR_INITIALIZATION,
                              (uint32_t)output_result);
        return MOTOR_CONTROL_INIT_BACKEND_ERROR;
    }

    control.outputs_stopped = true;
    control.initialized = true;
    return MOTOR_CONTROL_INIT_OK;
}

motor_control_submit_result_t motor_control_submit(
    const motor_command_t *logical_command)
{
    motor_command_t validated_command;
    motor_command_t physical_command;
    motor_output_submit_result_t output_result;

    if (!control.initialized) {
        return MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED;
    }
    if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        return stop_and_return(MOTOR_CONTROL_SUBMIT_BLOCKED_STATE);
    }
    if (!current_health_allows_output()) {
        (void)enter_failsafe_if_armed();
        return stop_and_return(MOTOR_CONTROL_SUBMIT_BLOCKED_HEALTH);
    }
    if ((logical_command == NULL) || !logical_command->valid ||
        (motor_command_create(&validated_command,
                              logical_command->throttle,
                              logical_command->timestamp_us) !=
         MOTOR_COMMAND_CREATE_OK)) {
        (void)enter_failsafe_if_armed();
        return stop_and_return(MOTOR_CONTROL_SUBMIT_INVALID_COMMAND);
    }
    if (!motor_command_is_fresh(&validated_command,
                                control.clock(),
                                control.command_timeout_us)) {
        (void)enter_failsafe_if_armed();
        return stop_and_return(MOTOR_CONTROL_SUBMIT_STALE_COMMAND);
    }
    if (motor_mapping_apply(&control.mapping,
                            &validated_command,
                            &physical_command) != MOTOR_MAPPING_APPLY_OK) {
        (void)enter_failsafe_if_armed();
        report_motor_fault(FAULT_ID_MOTOR_OUTPUT, 1U);
        return stop_and_return(MOTOR_CONTROL_SUBMIT_MAPPING_ERROR);
    }

    output_result = motor_output_submit(&control.physical_output,
                                        &physical_command);
    if (output_result == MOTOR_OUTPUT_SUBMIT_ACCEPTED) {
        control.last_accepted_command = physical_command;
        control.outputs_stopped = false;
        return MOTOR_CONTROL_SUBMIT_ACCEPTED;
    }
    if (output_result == MOTOR_OUTPUT_SUBMIT_BUSY) {
        return MOTOR_CONTROL_SUBMIT_BUSY;
    }

    (void)enter_failsafe_if_armed();
    report_motor_fault(FAULT_ID_MOTOR_OUTPUT,
                          (uint32_t)output_result);
    return stop_and_return(MOTOR_CONTROL_SUBMIT_BACKEND_ERROR);
}

motor_control_sync_result_t motor_control_synchronize(void)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_SYNC_NOT_INITIALIZED;
    }
    if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        if (control.outputs_stopped) {
            return MOTOR_CONTROL_SYNC_STOPPED;
        }
        return force_stop_internal() ? MOTOR_CONTROL_SYNC_STOPPED
                                     : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }
    if (!current_health_allows_output()) {
        (void)enter_failsafe_if_armed();
        if (control.outputs_stopped) {
            return MOTOR_CONTROL_SYNC_STOPPED;
        }
        return force_stop_internal() ? MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED
                                     : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }
    if (control.outputs_stopped) {
        return MOTOR_CONTROL_SYNC_STOPPED;
    }
    if (!motor_command_is_fresh(&control.last_accepted_command,
                                control.clock(),
                                control.command_timeout_us)) {
        (void)enter_failsafe_if_armed();
        return force_stop_internal() ? MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED
                                     : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }

    return MOTOR_CONTROL_SYNC_SAFE;
}

motor_control_stop_result_t motor_control_force_stop(void)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_STOP_NOT_INITIALIZED;
    }

    return force_stop_internal() ? MOTOR_CONTROL_STOP_ACCEPTED
                                 : MOTOR_CONTROL_STOP_ERROR;
}

motor_control_mapping_configure_result_t motor_control_configure_mapping(
    const uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT])
{
    motor_mapping_configure_result_t result;

    if (!control.initialized) {
        return MOTOR_CONTROL_MAPPING_CONFIGURE_NOT_INITIALIZED;
    }

    result = motor_mapping_configure(
        &control.mapping,
        logical_to_physical,
        control.state_machine->current == SYSTEM_STATE_DISARMED,
        control.outputs_stopped);
    switch (result) {
    case MOTOR_MAPPING_CONFIGURE_OK:
        return MOTOR_CONTROL_MAPPING_CONFIGURE_OK;
    case MOTOR_MAPPING_CONFIGURE_INVALID_ARGUMENT:
        return MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_ARGUMENT;
    case MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE:
        return MOTOR_CONTROL_MAPPING_CONFIGURE_UNSAFE_STATE;
    case MOTOR_MAPPING_CONFIGURE_INVALID_PERMUTATION:
        return MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_PERMUTATION;
    }

    return MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_ARGUMENT;
}

bool motor_control_is_initialized(void)
{
    return control.initialized;
}

bool motor_control_outputs_stopped(void)
{
    return control.initialized && control.outputs_stopped;
}
