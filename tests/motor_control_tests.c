#include "motor_control.h"
#include "motor_control_internal.h"
#include "fault_catalog.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define COMMAND_TIMEOUT_US UINT64_C(100000)
#define TEST_WARNING_FAULT_ID UINT16_C(100)

typedef struct {
    motor_output_backend_init_result_t initialize_result;
    motor_output_backend_submit_result_t submit_result;
    motor_output_backend_stop_result_t stop_result;
    motor_output_backend_status_t status_result;
    uint32_t diagnostic_context;
    motor_command_t last_command;
    uint32_t initialize_count;
    uint32_t submit_count;
    uint32_t stop_count;
    uint32_t status_count;
} fake_backend_t;

static uint64_t current_time_us;

static uint64_t fake_clock(void)
{
    return current_time_us;
}

static motor_output_backend_init_result_t fake_initialize(void *context)
{
    fake_backend_t *fake = context;

    fake->initialize_count++;
    return fake->initialize_result;
}

static motor_output_backend_submit_result_t fake_submit(
    const motor_command_t *command,
    void *context)
{
    fake_backend_t *fake = context;

    fake->submit_count++;
    if (fake->submit_result == MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED) {
        fake->last_command = *command;
    }
    return fake->submit_result;
}

static motor_output_backend_stop_result_t fake_force_stop(void *context)
{
    fake_backend_t *fake = context;

    fake->stop_count++;
    return fake->stop_result;
}

static motor_output_backend_status_t fake_status(void *context)
{
    fake_backend_t *fake = context;

    fake->status_count++;
    return fake->status_result;
}

static uint32_t fake_diagnostic_context(void *context)
{
    const fake_backend_t *fake = context;

    return fake->diagnostic_context;
}

static motor_output_backend_t backend_for(fake_backend_t *fake)
{
    return (motor_output_backend_t){
        .initialize = fake_initialize,
        .submit = fake_submit,
        .force_stop = fake_force_stop,
        .status = fake_status,
        .diagnostic_context = fake_diagnostic_context,
        .context = fake,
    };
}

static void initialize_fault_system(system_state_machine_t *state_machine,
                                    fault_system_t *fault_system)
{
    static fault_definition_t test_definitions[FAULT_DEFINITION_CAPACITY];
    const fault_definition_t *definitions;
    size_t definition_count;
    size_t index;

    system_state_machine_initialize(state_machine);
    definitions = firmware_fault_catalog(&definition_count);
    assert(definition_count < FAULT_DEFINITION_CAPACITY);
    for (index = 0U; index < definition_count; index++) {
        test_definitions[index] = definitions[index];
    }
    test_definitions[definition_count] = (fault_definition_t){
        .id = TEST_WARNING_FAULT_ID,
        .severity = FAULT_SEVERITY_WARNING,
        .source = FAULT_SOURCE_APPLICATION,
    };
    definition_count++;
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   test_definitions,
                                   definition_count) == FAULT_INIT_OK);
}

static void enter_disarmed(system_state_machine_t *state_machine)
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

static void enter_armed(system_state_machine_t *state_machine)
{
    assert(state_machine->current == SYSTEM_STATE_DISARMED);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST) ==
           MOTOR_CONTROL_ARM_ACCEPTED);
    assert(state_machine->current == SYSTEM_STATE_ARMED);
    assert(motor_control_active_source() ==
           MOTOR_CONTROL_SOURCE_USB_TEST);
}

static void leave_failsafe(system_state_machine_t *state_machine)
{
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
    assert(state_machine->current == SYSTEM_STATE_DISARMED);
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_NONE);
    assert(strcmp(motor_control_source_name(MOTOR_CONTROL_SOURCE_NONE),
                  "NONE") == 0);
    assert(strcmp(motor_control_source_name(MOTOR_CONTROL_SOURCE_USB_TEST),
                  "USB_TEST") == 0);
    assert(strcmp(motor_control_source_name(MOTOR_CONTROL_SOURCE_RECEIVER),
                  "RECEIVER") == 0);
    assert(strcmp(motor_control_source_name(MOTOR_CONTROL_SOURCE_COUNT),
                  "UNKNOWN") == 0);
}

static void prepare_arming(system_state_machine_t *state_machine,
                           fake_backend_t *fake)
{
    uint32_t submits_before = fake->submit_count;

    assert(state_machine->current == SYSTEM_STATE_DISARMED);
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    fake->stop_result = MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED;
    if (motor_control_ready_for_arm()) {
        return;
    }
    assert(!motor_control_ready_for_arm());
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 1U);
    assert(fake->last_command.throttle[0] == 0.0f);
    assert(fake->last_command.throttle[1] == 0.0f);
    assert(fake->last_command.throttle[2] == 0.0f);
    assert(fake->last_command.throttle[3] == 0.0f);
    assert(!motor_control_outputs_stopped());
    assert(!motor_control_ready_for_arm());

    current_time_us += MOTOR_CONTROL_ARMING_PREPARATION_US;
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 2U);
    assert(motor_control_ready_for_arm());
}

static motor_command_t make_command(float m0,
                                    float m1,
                                    float m2,
                                    float m3,
                                    uint64_t timestamp_us)
{
    const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {m0, m1, m2, m3};
    motor_command_t command;

    assert(motor_command_create(&command, throttle, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK);
    return command;
}

static void uninitialized_and_failed_initialization_are_fail_closed(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    static const fault_definition_t incomplete_definitions[] = {
        {TEST_WARNING_FAULT_ID,
         FAULT_SEVERITY_WARNING,
         FAULT_SOURCE_APPLICATION},
    };
    system_state_machine_t incomplete_state;
    fault_system_t incomplete_fault_system;
    motor_output_backend_t backend = backend_for(fake);
    const motor_command_t command =
        make_command(0.1f, 0.2f, 0.3f, 0.4f, 1U);

    assert(!motor_control_is_initialized());
    assert(!motor_control_outputs_stopped());
    assert(!motor_control_ready_for_arm());
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_NONE);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST) ==
           MOTOR_CONTROL_ARM_NOT_INITIALIZED);
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_NOT_INITIALIZED);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_NOT_INITIALIZED);
    assert(motor_control_force_stop() == MOTOR_CONTROL_STOP_NOT_INITIALIZED);
    assert(motor_control_configure_mapping(NULL) ==
           MOTOR_CONTROL_MAPPING_CONFIGURE_NOT_INITIALIZED);
    assert(motor_control_initialize(NULL,
                                       fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) ==
           MOTOR_CONTROL_INIT_INVALID_ARGUMENT);

    system_state_machine_initialize(&incomplete_state);
    assert(fault_system_initialize(&incomplete_fault_system,
                                   &incomplete_state,
                                   incomplete_definitions,
                                   1U) == FAULT_INIT_OK);
    assert(motor_control_initialize(&incomplete_state,
                                       &incomplete_fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) ==
           MOTOR_CONTROL_INIT_INVALID_ARGUMENT);

    fake->initialize_result = MOTOR_OUTPUT_BACKEND_INIT_ERROR;
    assert(motor_control_initialize(state_machine,
                                       fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) ==
           MOTOR_CONTROL_INIT_BACKEND_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(
               fault_system,
               FAULT_ID_MOTOR_INITIALIZATION) != NULL);

    initialize_fault_system(state_machine, fault_system);
    fake->initialize_result = MOTOR_OUTPUT_BACKEND_INIT_OK;
    fake->stop_result = MOTOR_OUTPUT_BACKEND_STOP_ERROR;
    assert(motor_control_initialize(state_machine,
                                       fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) ==
           MOTOR_CONTROL_INIT_INITIAL_STOP_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(fault_system,
                                      FAULT_ID_MOTOR_FORCE_STOP) != NULL);
}

static void successful_initialization_is_stopped_and_singleton(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_output_backend_t backend = backend_for(fake);

    initialize_fault_system(state_machine, fault_system);
    enter_disarmed(state_machine);
    fake->initialize_result = MOTOR_OUTPUT_BACKEND_INIT_OK;
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    fake->stop_result = MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED;
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;

    assert(motor_control_initialize(state_machine,
                                       fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) == MOTOR_CONTROL_INIT_OK);
    assert(motor_control_is_initialized());
    assert(motor_control_outputs_stopped());
    assert(!motor_control_ready_for_arm());
    assert(motor_control_initialize(state_machine,
                                       fault_system,
                                       fake_clock,
                                       COMMAND_TIMEOUT_US,
                                       &backend) ==
           MOTOR_CONTROL_INIT_ALREADY_INITIALIZED);
}

static void arming_is_blocked_before_stop_preparation(
    system_state_machine_t *state_machine,
    fake_backend_t *fake)
{
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_NONE) ==
           MOTOR_CONTROL_ARM_INVALID_SOURCE);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_COUNT) ==
           MOTOR_CONTROL_ARM_INVALID_SOURCE);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST) ==
           MOTOR_CONTROL_ARM_BLOCKED_PREPARATION);
    assert(state_machine->current == SYSTEM_STATE_DISARMED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->last_command.throttle[0] == 0.0f);
    assert(fake->last_command.throttle[1] == 0.0f);
    assert(fake->last_command.throttle[2] == 0.0f);
    assert(fake->last_command.throttle[3] == 0.0f);
    current_time_us += MOTOR_CONTROL_ARMING_PREPARATION_US;
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    enter_armed(state_machine);
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
}

static void mapping_is_private_and_requires_disarmed_output(
    system_state_machine_t *state_machine)
{
    static const uint8_t reverse[MOTOR_COMMAND_MOTOR_COUNT] = {3U, 2U, 1U, 0U};
    static const uint8_t duplicate[MOTOR_COMMAND_MOTOR_COUNT] = {0U, 0U, 2U, 3U};

    assert(motor_control_configure_mapping(NULL) ==
           MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_ARGUMENT);
    assert(motor_control_configure_mapping(duplicate) ==
           MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_PERMUTATION);
    assert(motor_control_configure_mapping(reverse) ==
           MOTOR_CONTROL_MAPPING_CONFIGURE_OK);

    enter_armed(state_machine);
    assert(motor_control_configure_mapping(reverse) ==
           MOTOR_CONTROL_MAPPING_CONFIGURE_UNSAFE_STATE);
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
}

static void command_source_is_exclusive_and_hands_off_after_disarm(
    system_state_machine_t *state_machine,
    fake_backend_t *fake)
{
    const motor_command_t owner_command =
        make_command(0.1f, 0.2f, 0.3f, 0.4f, current_time_us);
    const motor_command_t other_command =
        make_command(0.9f, 0.9f, 0.9f, 0.9f, current_time_us);

    enter_armed(state_machine);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_RECEIVER) ==
           MOTOR_CONTROL_ARM_BLOCKED_STATE);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST,
                                &owner_command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                &other_command) ==
           MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->last_command.throttle[0] == 0.4f);
    assert(fake->last_command.throttle[1] == 0.3f);
    assert(fake->last_command.throttle[2] == 0.2f);
    assert(fake->last_command.throttle[3] == 0.1f);

    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_NONE);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST,
                                &owner_command) ==
           MOTOR_CONTROL_SUBMIT_BLOCKED_STATE);

    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_RECEIVER) ==
           MOTOR_CONTROL_ARM_ACCEPTED);
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_RECEIVER);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST,
                                &owner_command) ==
           MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                &owner_command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
}

static void allowed_health_passes_fresh_commands_with_mapping(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_command_t command;
    uint32_t submits_before;

    prepare_arming(state_machine, fake);
    enter_armed(state_machine);

    current_time_us += UINT64_C(500000);
    command = make_command(0.1f, 0.2f, 0.3f, 0.4f, current_time_us);
    submits_before = fake->submit_count;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(fake->submit_count == submits_before);
    assert(!motor_control_outputs_stopped());
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 1U);
    assert(fake->last_command.throttle[0] == 0.4f);
    assert(fake->last_command.throttle[1] == 0.3f);
    assert(fake->last_command.throttle[2] == 0.2f);
    assert(fake->last_command.throttle[3] == 0.1f);
    assert(!motor_control_outputs_stopped());

    assert(fault_system_report(fault_system,
                               TEST_WARNING_FAULT_ID,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(fault_system_report(fault_system,
                               FAULT_ID_LOGGING_CLOCK_ATTACHMENT,
                               false,
                               0U) == FAULT_REPORT_RECORDED);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);

    submits_before = fake->submit_count;
    assert(motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_BLOCKED_STATE);
    assert(fake->submit_count == submits_before);
    assert(!motor_control_outputs_stopped());
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->last_command.throttle[0] == 0.0f);
}

static void invalid_and_stale_commands_enter_failsafe(
    system_state_machine_t *state_machine,
    fake_backend_t *fake)
{
    motor_command_t command;
    uint32_t stop_before;

    enter_armed(state_machine);
    current_time_us = UINT64_C(700000);
    command = make_command(0.2f, 0.2f, 0.2f, 0.2f, current_time_us);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    command.throttle[2] = NAN;
    stop_before = fake->stop_count;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_INVALID_COMMAND);
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_NONE);
    assert(fake->stop_count == stop_before + 1U);
    assert(motor_control_outputs_stopped());

    leave_failsafe(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    command = make_command(0.3f, 0.3f, 0.3f, 0.3f,
                           current_time_us - COMMAND_TIMEOUT_US - 1U);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_STALE_COMMAND);
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(motor_control_outputs_stopped());
}

static void periodic_service_repeats_the_latest_fresh_command(
    system_state_machine_t *state_machine,
    fake_backend_t *fake)
{
    motor_command_t command;
    uint32_t submits_before;

    leave_failsafe(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    current_time_us = UINT64_C(900000);
    command = make_command(0.4f, 0.4f, 0.4f, 0.4f, current_time_us);
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    submits_before = fake->submit_count;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(fake->submit_count == submits_before);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 1U);
    assert(fake->last_command.timestamp_us == UINT64_C(900000));

    current_time_us += UINT64_C(1000);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 2U);
    assert(fake->last_command.timestamp_us == UINT64_C(900000));

    current_time_us += UINT64_C(1000);
    command = make_command(0.5f, 0.5f, 0.5f, 0.5f, current_time_us);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(fake->submit_count == submits_before + 2U);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 3U);
    assert(fake->last_command.throttle[0] == 0.5f);
    assert(fake->last_command.timestamp_us == current_time_us);

    current_time_us += COMMAND_TIMEOUT_US;
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    assert(fake->submit_count == submits_before + 4U);
    current_time_us++;
    assert(motor_control_synchronize() ==
           MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED);
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(!motor_control_outputs_stopped());
}

static void stuck_busy_output_becomes_critical(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_command_t command;

    initialize_fault_system(state_machine, fault_system);
    enter_disarmed(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    current_time_us = UINT64_C(1050000);
    command = make_command(0.5f, 0.5f, 0.5f, 0.5f, current_time_us);
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);

    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_BUSY;
    current_time_us += MOTOR_CONTROL_OUTPUT_COMPLETION_TIMEOUT_US / 2U;
    command = make_command(0.6f, 0.6f, 0.6f, 0.6f, current_time_us);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    current_time_us +=
        (MOTOR_CONTROL_OUTPUT_COMPLETION_TIMEOUT_US / 2U) - 1U;
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    current_time_us++;
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_BACKEND_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(fault_system,
                                      FAULT_ID_MOTOR_OUTPUT) != NULL);
    assert(motor_control_outputs_stopped());
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
}

static void unknown_health_stops_and_enters_failsafe(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_command_t command;

    leave_failsafe(state_machine);
    prepare_arming(state_machine, fake);
    fault_system->dropped_record_count = 1U;
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST) ==
           MOTOR_CONTROL_ARM_BLOCKED_HEALTH);
    assert(state_machine->current == SYSTEM_STATE_DISARMED);
    assert(motor_control_active_source() == MOTOR_CONTROL_SOURCE_NONE);
    fault_system->dropped_record_count = 0U;
    enter_armed(state_machine);
    current_time_us = UINT64_C(1100000);
    command = make_command(0.6f, 0.6f, 0.6f, 0.6f, current_time_us);
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);

    fault_system->dropped_record_count = 1U;
    assert(motor_control_synchronize() ==
           MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED);
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(!motor_control_outputs_stopped());
    fault_system->dropped_record_count = 0U;

    leave_failsafe(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    fault_system->dropped_record_count = 1U;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_BLOCKED_HEALTH);
    assert(state_machine->current == SYSTEM_STATE_FAILSAFE);
    assert(motor_control_outputs_stopped());
    fault_system->dropped_record_count = 0U;
}

static void backend_and_stop_failures_become_critical(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_command_t command;

    leave_failsafe(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    current_time_us = UINT64_C(1300000);
    command = make_command(0.7f, 0.7f, 0.7f, 0.7f, current_time_us);
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_BACKEND_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(fault_system,
                                      FAULT_ID_MOTOR_OUTPUT) != NULL);

    /* Reinitialize the referenced state/fault objects to isolate stop failure. */
    initialize_fault_system(state_machine, fault_system);
    enter_disarmed(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    command = make_command(0.7f, 0.7f, 0.7f, 0.7f, current_time_us);
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);
    fake->stop_result = MOTOR_OUTPUT_BACKEND_STOP_ERROR;
    assert(motor_control_force_stop() == MOTOR_CONTROL_STOP_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(fault_system,
                                      FAULT_ID_MOTOR_FORCE_STOP) != NULL);
    assert(!motor_control_outputs_stopped());
}

static void asynchronous_backend_error_becomes_critical(
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    fake_backend_t *fake)
{
    motor_command_t command;

    initialize_fault_system(state_machine, fault_system);
    enter_disarmed(state_machine);
    prepare_arming(state_machine, fake);
    enter_armed(state_machine);
    current_time_us = UINT64_C(1500000);
    command = make_command(0.4f, 0.3f, 0.2f, 0.1f, current_time_us);
    fake->submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    fake->stop_result = MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED;
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    assert(motor_control_submit(MOTOR_CONTROL_SOURCE_USB_TEST, &command) ==
           MOTOR_CONTROL_SUBMIT_ACCEPTED);
    assert(motor_control_synchronize() == MOTOR_CONTROL_SYNC_SAFE);

    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_ERROR;
    fake->diagnostic_context = UINT32_C(108);
    assert(motor_control_synchronize() ==
           MOTOR_CONTROL_SYNC_BACKEND_ERROR);
    assert(state_machine->current == SYSTEM_STATE_FAULT);
    assert(fault_system_record_for_id(fault_system,
                                      FAULT_ID_MOTOR_OUTPUT)->context ==
           UINT32_C(108));
    assert(motor_control_outputs_stopped());
    fake->status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    fake->diagnostic_context = 0U;
}

int main(void)
{
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    fake_backend_t fake = {
        .initialize_result = MOTOR_OUTPUT_BACKEND_INIT_OK,
        .submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED,
        .stop_result = MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED,
        .status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE,
    };

    initialize_fault_system(&state_machine, &fault_system);
    uninitialized_and_failed_initialization_are_fail_closed(
        &state_machine, &fault_system, &fake);
    successful_initialization_is_stopped_and_singleton(
        &state_machine, &fault_system, &fake);
    arming_is_blocked_before_stop_preparation(
        &state_machine, &fake);
    mapping_is_private_and_requires_disarmed_output(&state_machine);
    command_source_is_exclusive_and_hands_off_after_disarm(&state_machine,
                                                           &fake);
    allowed_health_passes_fresh_commands_with_mapping(
        &state_machine, &fault_system, &fake);
    invalid_and_stale_commands_enter_failsafe(&state_machine, &fake);
    periodic_service_repeats_the_latest_fresh_command(&state_machine, &fake);
    unknown_health_stops_and_enters_failsafe(
        &state_machine, &fault_system, &fake);
    backend_and_stop_failures_become_critical(
        &state_machine, &fault_system, &fake);
    asynchronous_backend_error_becomes_critical(
        &state_machine, &fault_system, &fake);
    stuck_busy_output_becomes_critical(
        &state_machine, &fault_system, &fake);
    return 0;
}
