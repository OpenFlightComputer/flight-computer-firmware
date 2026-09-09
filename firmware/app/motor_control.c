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
    motor_configuration_t configuration;
    motor_command_t retained_command;
    motor_control_source_t active_source;
    motor_control_source_t pending_source;
    uint64_t transfer_started_at_us;
    uint64_t stop_stream_started_at_us;
    bool transfer_in_progress;
    bool stop_stream_started;
    bool arming_preparation_complete;
    uint8_t direction_repetitions_remaining;
    bool direction_sequence_active;
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

static uint32_t output_fault_context(uint32_t fallback)
{
    const uint32_t context =
        motor_output_diagnostic_context(&control.physical_output);

    return context != 0U ? context : fallback;
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
        control.active_source = MOTOR_CONTROL_SOURCE_NONE;
        motor_command_invalidate(&control.retained_command);
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

    control.active_source = MOTOR_CONTROL_SOURCE_NONE;
    control.pending_source = MOTOR_CONTROL_SOURCE_NONE;
    control.direction_sequence_active = false;
    control.direction_repetitions_remaining = 0U;
    control.transfer_in_progress = false;
    control.stop_stream_started = false;
    control.arming_preparation_complete = false;
    motor_command_invalidate(&control.retained_command);

    if (result != MOTOR_OUTPUT_STOP_ACCEPTED) {
        control.outputs_stopped = false;
        report_motor_fault(FAULT_ID_MOTOR_FORCE_STOP,
                           output_fault_context((uint32_t)result));
        return false;
    }

    control.outputs_stopped = true;
    return true;
}

static void start_direction_sequence(void)
{
    control.direction_repetitions_remaining =
        MOTOR_CONTROL_DIRECTION_COMMAND_REPETITIONS;
    control.direction_sequence_active = true;
}

static bool map_directions_to_physical(
    motor_direction_t physical[MOTOR_COMMAND_MOTOR_COUNT])
{
    bool seen[MOTOR_COMMAND_MOTOR_COUNT] = {false};
    size_t logical_motor;

    if (!motor_configuration_is_valid(&control.configuration) ||
        !motor_mapping_is_valid(&control.mapping)) {
        return false;
    }

    for (logical_motor = 0U;
         logical_motor < MOTOR_COMMAND_MOTOR_COUNT;
         logical_motor++) {
        const uint8_t physical_output =
            control.mapping.logical_to_physical[logical_motor];

        if ((physical_output >= MOTOR_COMMAND_MOTOR_COUNT) ||
            seen[physical_output]) {
            return false;
        }
        physical[physical_output] =
            control.configuration.direction[logical_motor];
        seen[physical_output] = true;
    }
    return true;
}

static bool complete_pending_arm(void)
{
    system_state_transition_result_t transition_result;
    const motor_control_source_t source = control.pending_source;

    if (source == MOTOR_CONTROL_SOURCE_NONE) {
        return true;
    }
    control.pending_source = MOTOR_CONTROL_SOURCE_NONE;
    if ((control.state_machine->current != SYSTEM_STATE_DISARMED) ||
        !motor_fault_state_allows_arm(control.fault_system) ||
        !control.arming_preparation_complete) {
        return true;
    }

    transition_result = system_state_machine_handle_event(
        control.state_machine,
        SYSTEM_STATE_EVENT_ARM_REQUESTED);
    if (transition_result != SYSTEM_STATE_TRANSITION_OK) {
        report_motor_fault(FAULT_ID_STATE_MACHINE_TRANSITION,
                           (uint32_t)transition_result);
        return false;
    }

    control.active_source = source;
    motor_command_invalidate(&control.retained_command);
    return true;
}

static bool command_is_stop(const motor_command_t *command)
{
    size_t motor;

    if ((command == NULL) || !command->valid) {
        return false;
    }
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        if (command->throttle[motor] != 0.0f) {
            return false;
        }
    }
    return true;
}

static bool create_stop_command(uint64_t timestamp_us,
                                motor_command_t *command)
{
    const float zero_throttle[MOTOR_COMMAND_MOTOR_COUNT] = {0.0f};

    return motor_command_create(command, zero_throttle, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK;
}

static void record_accepted_stop_frame(uint64_t now_us)
{
    if (!control.stop_stream_started ||
        (now_us < control.stop_stream_started_at_us)) {
        control.stop_stream_started_at_us = now_us;
        control.stop_stream_started = true;
    }
    if ((now_us - control.stop_stream_started_at_us) >=
        MOTOR_CONTROL_ARMING_PREPARATION_US) {
        control.arming_preparation_complete = true;
    }
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
    const motor_output_backend_t *backend,
    const motor_configuration_t *configuration)
{
    motor_output_init_result_t output_result;

    if (control.initialized) {
        return MOTOR_CONTROL_INIT_ALREADY_INITIALIZED;
    }
    if ((state_machine == NULL) || !state_machine->initialized ||
        (fault_system == NULL) || !fault_system->initialized ||
        (fault_system->state_machine != state_machine) || (clock == NULL) ||
        (command_timeout_us == 0U) || (backend == NULL) ||
        !motor_configuration_is_valid(configuration) ||
        !motor_fault_policy_is_valid(fault_system)) {
        return MOTOR_CONTROL_INIT_INVALID_ARGUMENT;
    }

    control = (motor_control_state_t){
        .state_machine = state_machine,
        .fault_system = fault_system,
        .clock = clock,
        .command_timeout_us = command_timeout_us,
        .configuration = *configuration,
    };
    motor_mapping_initialize(&control.mapping);
    motor_command_initialize(&control.retained_command);

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

motor_control_arm_result_t motor_control_arm(motor_control_source_t source)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_ARM_NOT_INITIALIZED;
    }
    if ((source <= MOTOR_CONTROL_SOURCE_NONE) ||
        (source >= MOTOR_CONTROL_SOURCE_COUNT)) {
        return MOTOR_CONTROL_ARM_INVALID_SOURCE;
    }
    if ((control.state_machine->current != SYSTEM_STATE_DISARMED) ||
        (control.active_source != MOTOR_CONTROL_SOURCE_NONE) ||
        (control.pending_source != MOTOR_CONTROL_SOURCE_NONE)) {
        return MOTOR_CONTROL_ARM_BLOCKED_STATE;
    }
    if (!motor_fault_state_allows_arm(control.fault_system)) {
        return MOTOR_CONTROL_ARM_BLOCKED_HEALTH;
    }
    if (!motor_control_ready_for_arm()) {
        return MOTOR_CONTROL_ARM_BLOCKED_PREPARATION;
    }

    control.pending_source = source;
    motor_command_invalidate(&control.retained_command);
    start_direction_sequence();
    return MOTOR_CONTROL_ARM_PENDING;
}

motor_control_disarm_result_t motor_control_disarm(void)
{
    system_state_transition_result_t transition_result;

    if (!control.initialized) {
        return MOTOR_CONTROL_DISARM_NOT_INITIALIZED;
    }

    if ((control.state_machine->current == SYSTEM_STATE_DISARMED) &&
        (control.pending_source != MOTOR_CONTROL_SOURCE_NONE)) {
        control.pending_source = MOTOR_CONTROL_SOURCE_NONE;
        return MOTOR_CONTROL_DISARM_ACCEPTED;
    }

    transition_result = system_state_machine_handle_event(
        control.state_machine,
        SYSTEM_STATE_EVENT_DISARM_REQUESTED);
    if (transition_result == SYSTEM_STATE_TRANSITION_REJECTED) {
        return MOTOR_CONTROL_DISARM_BLOCKED_STATE;
    }
    if (transition_result != SYSTEM_STATE_TRANSITION_OK) {
        return MOTOR_CONTROL_DISARM_TRANSITION_ERROR;
    }

    control.active_source = MOTOR_CONTROL_SOURCE_NONE;
    motor_command_invalidate(&control.retained_command);
    return MOTOR_CONTROL_DISARM_ACCEPTED;
}

motor_control_submit_result_t motor_control_submit(
    motor_control_source_t source,
    const motor_command_t *logical_command)
{
    motor_command_t validated_command;
    motor_command_t physical_command;

    if (!control.initialized) {
        return MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED;
    }
    if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        control.active_source = MOTOR_CONTROL_SOURCE_NONE;
        motor_command_invalidate(&control.retained_command);
        return MOTOR_CONTROL_SUBMIT_BLOCKED_STATE;
    }
    if ((source <= MOTOR_CONTROL_SOURCE_NONE) ||
        (source >= MOTOR_CONTROL_SOURCE_COUNT) ||
        (source != control.active_source)) {
        return MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE;
    }
    if (!control.arming_preparation_complete) {
        return MOTOR_CONTROL_SUBMIT_BLOCKED_PREPARATION;
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

    /*
     * Command producers renew the safety lease and replace one complete
     * snapshot. The 1 kHz synchronization task alone owns physical frame
     * submission, so USB/radio timing cannot become the DShot frame rate.
     */
    control.retained_command = physical_command;
    return MOTOR_CONTROL_SUBMIT_ACCEPTED;
}

motor_control_sync_result_t motor_control_synchronize(void)
{
    motor_output_status_t output_status;
    motor_output_submit_result_t submit_result;
    motor_command_t stop_command;
    const motor_command_t *command_to_submit;
    uint64_t now_us;
    bool failsafe_entered = false;

    if (!control.initialized) {
        return MOTOR_CONTROL_SYNC_NOT_INITIALIZED;
    }

    output_status = motor_output_status(&control.physical_output);
    if ((output_status != MOTOR_OUTPUT_STATUS_IDLE) &&
        (output_status != MOTOR_OUTPUT_STATUS_BUSY)) {
        (void)enter_failsafe_if_armed();
        report_motor_fault(FAULT_ID_MOTOR_OUTPUT,
                           output_fault_context((uint32_t)output_status));
        if (!force_stop_internal()) {
            return MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
        }
        return MOTOR_CONTROL_SYNC_BACKEND_ERROR;
    }

    now_us = control.clock();
    if (output_status == MOTOR_OUTPUT_STATUS_IDLE) {
        control.transfer_in_progress = false;
    }

    if ((control.state_machine->current == SYSTEM_STATE_BOOT) ||
        (control.state_machine->current == SYSTEM_STATE_INITIALIZING) ||
        (control.state_machine->current == SYSTEM_STATE_FAULT)) {
        control.active_source = MOTOR_CONTROL_SOURCE_NONE;
        control.pending_source = MOTOR_CONTROL_SOURCE_NONE;
        control.direction_sequence_active = false;
        motor_command_invalidate(&control.retained_command);
        if (control.outputs_stopped) {
            return MOTOR_CONTROL_SYNC_STOPPED;
        }
        return force_stop_internal() ? MOTOR_CONTROL_SYNC_STOPPED
                                     : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }

    if (!create_stop_command(now_us, &stop_command)) {
        report_motor_fault(FAULT_ID_MOTOR_OUTPUT, 2U);
        return force_stop_internal() ? MOTOR_CONTROL_SYNC_BACKEND_ERROR
                                     : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }
    command_to_submit = &stop_command;

    if ((control.state_machine->current == SYSTEM_STATE_ARMED) &&
        !current_health_allows_output()) {
        (void)enter_failsafe_if_armed();
        motor_command_invalidate(&control.retained_command);
        failsafe_entered = true;
    } else if ((control.state_machine->current == SYSTEM_STATE_ARMED) &&
               control.arming_preparation_complete &&
               control.retained_command.valid &&
               !motor_command_is_fresh(&control.retained_command,
                                       now_us,
                                       control.command_timeout_us)) {
        (void)enter_failsafe_if_armed();
        motor_command_invalidate(&control.retained_command);
        failsafe_entered = true;
    } else if ((control.state_machine->current == SYSTEM_STATE_ARMED) &&
               control.arming_preparation_complete &&
               control.retained_command.valid) {
        command_to_submit = &control.retained_command;
    } else if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        control.active_source = MOTOR_CONTROL_SOURCE_NONE;
        motor_command_invalidate(&control.retained_command);
    }

    if (output_status == MOTOR_OUTPUT_STATUS_BUSY) {
        if (!control.transfer_in_progress ||
            (now_us < control.transfer_started_at_us) ||
            ((now_us - control.transfer_started_at_us) >=
             MOTOR_CONTROL_OUTPUT_COMPLETION_TIMEOUT_US)) {
            (void)enter_failsafe_if_armed();
            report_motor_fault(FAULT_ID_MOTOR_OUTPUT,
                               output_fault_context(
                                   (uint32_t)output_status));
            if (!force_stop_internal()) {
                return MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
            }
            return MOTOR_CONTROL_SYNC_BACKEND_ERROR;
        }
        return failsafe_entered ? MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED
                                : MOTOR_CONTROL_SYNC_SAFE;
    }

    if ((control.state_machine->current == SYSTEM_STATE_DISARMED) &&
        control.direction_sequence_active) {
        motor_direction_t physical_directions[MOTOR_COMMAND_MOTOR_COUNT];
        motor_output_direction_result_t direction_result;

        if (control.direction_repetitions_remaining == 0U) {
            control.direction_sequence_active = false;
            if (!complete_pending_arm()) {
                return MOTOR_CONTROL_SYNC_BACKEND_ERROR;
            }
        } else {
            if (!map_directions_to_physical(physical_directions)) {
                report_motor_fault(FAULT_ID_MOTOR_OUTPUT, 3U);
                return force_stop_internal()
                           ? MOTOR_CONTROL_SYNC_BACKEND_ERROR
                           : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
            }
            direction_result = motor_output_submit_directions(
                &control.physical_output,
                physical_directions);
            if (direction_result != MOTOR_OUTPUT_DIRECTION_ACCEPTED) {
                report_motor_fault(FAULT_ID_MOTOR_OUTPUT,
                                   output_fault_context(
                                       (uint32_t)direction_result));
                return force_stop_internal()
                           ? MOTOR_CONTROL_SYNC_BACKEND_ERROR
                           : MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
            }
            control.direction_repetitions_remaining--;
            control.transfer_started_at_us = now_us;
            control.transfer_in_progress = true;
            control.outputs_stopped = false;
            return MOTOR_CONTROL_SYNC_SAFE;
        }
    }

    submit_result = motor_output_submit(&control.physical_output,
                                        command_to_submit);
    if (submit_result == MOTOR_OUTPUT_SUBMIT_ACCEPTED) {
        control.transfer_started_at_us = now_us;
        control.transfer_in_progress = true;
        control.outputs_stopped = false;
        if (command_is_stop(command_to_submit)) {
            record_accepted_stop_frame(now_us);
        }
        return failsafe_entered ? MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED
                                : MOTOR_CONTROL_SYNC_SAFE;
    }

    (void)enter_failsafe_if_armed();
    report_motor_fault(FAULT_ID_MOTOR_OUTPUT,
                       output_fault_context((uint32_t)submit_result));
    if (!force_stop_internal()) {
        return MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR;
    }
    return MOTOR_CONTROL_SYNC_BACKEND_ERROR;
}

motor_control_stop_result_t motor_control_force_stop(void)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_STOP_NOT_INITIALIZED;
    }

    return force_stop_internal() ? MOTOR_CONTROL_STOP_ACCEPTED
                                 : MOTOR_CONTROL_STOP_ERROR;
}

motor_control_failsafe_result_t motor_control_enter_failsafe(void)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_FAILSAFE_NOT_INITIALIZED;
    }
    if (control.state_machine->current != SYSTEM_STATE_ARMED) {
        return MOTOR_CONTROL_FAILSAFE_NOT_ARMED;
    }
    return enter_failsafe_if_armed()
               ? MOTOR_CONTROL_FAILSAFE_ACCEPTED
               : MOTOR_CONTROL_FAILSAFE_TRANSITION_ERROR;
}

motor_control_recovery_result_t motor_control_recover_to_disarmed(void)
{
    motor_control_disarm_result_t result;

    if (!control.initialized) {
        return MOTOR_CONTROL_RECOVERY_NOT_INITIALIZED;
    }
    if ((control.state_machine->current == SYSTEM_STATE_DISARMED) &&
        (control.active_source == MOTOR_CONTROL_SOURCE_NONE) &&
        (control.pending_source == MOTOR_CONTROL_SOURCE_NONE)) {
        return MOTOR_CONTROL_RECOVERY_ACCEPTED;
    }
    if (control.state_machine->current != SYSTEM_STATE_FAILSAFE) {
        return MOTOR_CONTROL_RECOVERY_BLOCKED_STATE;
    }
    result = motor_control_disarm();
    if (result == MOTOR_CONTROL_DISARM_ACCEPTED) {
        return MOTOR_CONTROL_RECOVERY_ACCEPTED;
    }
    return result == MOTOR_CONTROL_DISARM_TRANSITION_ERROR
               ? MOTOR_CONTROL_RECOVERY_TRANSITION_ERROR
               : MOTOR_CONTROL_RECOVERY_BLOCKED_STATE;
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
        control.state_machine->current == SYSTEM_STATE_DISARMED);
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

motor_control_configuration_apply_result_t motor_control_apply_configuration(
    const motor_configuration_t *configuration)
{
    if (!control.initialized) {
        return MOTOR_CONTROL_CONFIGURATION_APPLY_NOT_INITIALIZED;
    }
    if (!motor_configuration_is_valid(configuration)) {
        return MOTOR_CONTROL_CONFIGURATION_APPLY_INVALID_ARGUMENT;
    }
    if ((control.state_machine->current != SYSTEM_STATE_DISARMED) ||
        (control.pending_source != MOTOR_CONTROL_SOURCE_NONE)) {
        return MOTOR_CONTROL_CONFIGURATION_APPLY_UNSAFE_STATE;
    }

    control.configuration = *configuration;
    start_direction_sequence();
    return MOTOR_CONTROL_CONFIGURATION_APPLY_OK;
}

bool motor_control_is_initialized(void)
{
    return control.initialized;
}

bool motor_control_outputs_stopped(void)
{
    return control.initialized && control.outputs_stopped;
}

bool motor_control_ready_for_arm(void)
{
    uint64_t now_us;

    if (!control.initialized ||
        (control.state_machine->current != SYSTEM_STATE_DISARMED) ||
        control.outputs_stopped || !control.stop_stream_started) {
        return false;
    }

    now_us = control.clock();
    if (now_us < control.stop_stream_started_at_us) {
        return false;
    }
    if ((now_us - control.stop_stream_started_at_us) >=
        MOTOR_CONTROL_ARMING_PREPARATION_US) {
        control.arming_preparation_complete = true;
    }
    return control.arming_preparation_complete;
}

motor_control_source_t motor_control_active_source(void)
{
    if (!control.initialized ||
        (control.state_machine->current != SYSTEM_STATE_ARMED)) {
        return MOTOR_CONTROL_SOURCE_NONE;
    }
    return control.active_source;
}

motor_control_source_t motor_control_pending_source(void)
{
    if (!control.initialized ||
        (control.state_machine->current != SYSTEM_STATE_DISARMED)) {
        return MOTOR_CONTROL_SOURCE_NONE;
    }
    return control.pending_source;
}

const char *motor_control_source_name(motor_control_source_t source)
{
    switch (source) {
    case MOTOR_CONTROL_SOURCE_NONE:
        return "NONE";
    case MOTOR_CONTROL_SOURCE_USB_TEST:
        return "USB_TEST";
    case MOTOR_CONTROL_SOURCE_RECEIVER:
        return "RECEIVER";
    case MOTOR_CONTROL_SOURCE_COUNT:
        break;
    }
    return "UNKNOWN";
}
