#include "fault.h"
#include "logging.h"
#include "motor_control.h"
#include "system_state.h"
#include "usb_cdc_transport.h"
#include "usb_command_processor.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define INPUT_CAPACITY 4U

static const char *input_lines[INPUT_CAPACITY];
static size_t input_head;
static size_t input_count;
static usb_cdc_write_result_t write_result;
static char captured_response[USB_CDC_TRANSMIT_CAPACITY];
static size_t captured_length;
static size_t write_count;
static uint64_t current_time_us;
static motor_control_submit_result_t motor_submit_result;
static motor_control_arm_result_t motor_arm_result;
static motor_control_disarm_result_t motor_disarm_result;
static motor_control_source_t active_motor_source;
static system_state_machine_t *motor_state_machine;
static motor_command_t captured_motor_command;
static uint32_t motor_submit_count;
static bool motor_ready_for_arm;
static bool motor_outputs_stopped;
static receiver_inspection_t receiver_inspection;
static bool receiver_inspection_read_result;
static uint32_t receiver_inspection_read_count;

motor_control_submit_result_t motor_control_submit(
    motor_control_source_t source,
    const motor_command_t *command)
{
    assert(command != NULL);
    if (source != active_motor_source) {
        return MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE;
    }
    captured_motor_command = *command;
    motor_submit_count++;
    return motor_submit_result;
}

motor_control_arm_result_t motor_control_arm(motor_control_source_t source)
{
    if (motor_arm_result != MOTOR_CONTROL_ARM_ACCEPTED) {
        return motor_arm_result;
    }
    if ((motor_state_machine == NULL) ||
        (system_state_machine_handle_event(
             motor_state_machine,
             SYSTEM_STATE_EVENT_ARM_REQUESTED) !=
         SYSTEM_STATE_TRANSITION_OK)) {
        return MOTOR_CONTROL_ARM_BLOCKED_STATE;
    }
    active_motor_source = source;
    return MOTOR_CONTROL_ARM_ACCEPTED;
}

motor_control_disarm_result_t motor_control_disarm(void)
{
    if (motor_disarm_result != MOTOR_CONTROL_DISARM_ACCEPTED) {
        return motor_disarm_result;
    }
    if ((motor_state_machine == NULL) ||
        (system_state_machine_handle_event(
             motor_state_machine,
             SYSTEM_STATE_EVENT_DISARM_REQUESTED) !=
         SYSTEM_STATE_TRANSITION_OK)) {
        return MOTOR_CONTROL_DISARM_BLOCKED_STATE;
    }
    active_motor_source = MOTOR_CONTROL_SOURCE_NONE;
    return MOTOR_CONTROL_DISARM_ACCEPTED;
}

motor_control_source_t motor_control_active_source(void)
{
    return active_motor_source;
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

bool motor_control_ready_for_arm(void)
{
    return motor_ready_for_arm;
}

bool motor_control_outputs_stopped(void)
{
    return motor_outputs_stopped;
}

static bool fake_receiver_inspection_read(
    void *context,
    receiver_inspection_t *inspection)
{
    assert(context == &receiver_inspection);
    assert(inspection != NULL);
    receiver_inspection_read_count++;
    if (!receiver_inspection_read_result) {
        return false;
    }
    *inspection = receiver_inspection;
    return true;
}

usb_cdc_line_result_t usb_cdc_transport_read_line(uint8_t *destination,
                                                  size_t capacity,
                                                  size_t *length)
{
    const char *line;
    size_t line_length;

    if (input_count == 0U) {
        *length = 0U;
        return USB_CDC_LINE_UNAVAILABLE;
    }

    line = input_lines[input_head];
    line_length = strlen(line);
    assert(capacity > line_length);
    memcpy(destination, line, line_length + 1U);
    *length = line_length;
    input_head = (input_head + 1U) % INPUT_CAPACITY;
    input_count--;
    return USB_CDC_LINE_AVAILABLE;
}

usb_cdc_write_result_t usb_cdc_transport_try_write(const uint8_t *data,
                                                   size_t length)
{
    write_count++;
    assert(length < sizeof(captured_response));
    memcpy(captured_response, data, length);
    captured_response[length] = '\0';
    captured_length = length;
    return write_result;
}

static uint64_t fake_clock(void)
{
    return current_time_us;
}

static void queue_input(const char *line)
{
    assert(input_count < INPUT_CAPACITY);
    input_lines[(input_head + input_count) % INPUT_CAPACITY] = line;
    input_count++;
}

static void reset_fakes(void)
{
    input_head = 0U;
    input_count = 0U;
    write_result = USB_CDC_WRITE_ACCEPTED;
    captured_response[0] = '\0';
    captured_length = 0U;
    write_count = 0U;
    current_time_us = UINT64_C(123456);
    motor_submit_result = MOTOR_CONTROL_SUBMIT_ACCEPTED;
    motor_arm_result = MOTOR_CONTROL_ARM_ACCEPTED;
    motor_disarm_result = MOTOR_CONTROL_DISARM_ACCEPTED;
    active_motor_source = MOTOR_CONTROL_SOURCE_NONE;
    motor_state_machine = NULL;
    motor_command_initialize(&captured_motor_command);
    motor_submit_count = 0U;
    motor_ready_for_arm = true;
    motor_outputs_stopped = false;
    receiver_inspection = (receiver_inspection_t){
        .freshness = RECEIVER_FRESHNESS_UNAVAILABLE,
        .failsafe_state = RECEIVER_FAILSAFE_UNAVAILABLE,
        .failsafe_action = RECEIVER_FAILSAFE_ACTION_NONE,
    };
    receiver_inspection_read_result = true;
    receiver_inspection_read_count = 0U;
    logging_initialize();
}

static void initialize_system(usb_command_processor_t *processor,
                              system_state_machine_t *state_machine,
                              fault_system_t *fault_system)
{
    static const fault_definition_t definitions[] = {
        {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_USB},
    };
    const receiver_inspection_provider_t receiver_provider = {
        .read = fake_receiver_inspection_read,
        .context = &receiver_inspection,
    };

    system_state_machine_initialize(state_machine);
    motor_state_machine = state_machine;
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   definitions,
                                   1U) == FAULT_INIT_OK);
    assert(usb_command_processor_initialize(processor,
                                            state_machine,
                                            fault_system,
                                            fake_clock,
                                            &receiver_provider,
                                            "0.1.0",
                                            "test-build") ==
           USB_COMMAND_INIT_OK);
}

static void receiver_inspection_is_read_only_when_requested(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    receiver_inspection.available = true;
    receiver_inspection.freshness = RECEIVER_FRESHNESS_FRESH;
    receiver_inspection.failsafe_state = RECEIVER_FAILSAFE_LIVE;
    receiver_inspection.failsafe_action = RECEIVER_FAILSAFE_ACTION_LIVE;
    receiver_inspection.sequence = 9U;
    receiver_inspection.channels[0] = 992U;
    receiver_inspection.roll = 0.0F;
    receiver_inspection.pitch = 0.0F;
    receiver_inspection.yaw = 0.0F;
    receiver_inspection.throttle = 0.0F;
    initialize_system(&processor, &state_machine, &fault_system);

    queue_input("{\"type\":\"command\",\"request_id\":12,"
                "\"command\":\"status\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(receiver_inspection_read_count == 0U);

    queue_input("{\"type\":\"command\",\"request_id\":13,"
                "\"command\":\"receiver\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(receiver_inspection_read_count == 1U);
    assert(strstr(captured_response, "\"command\":\"receiver\"") != NULL);
    assert(strstr(captured_response, "\"available\":true") != NULL);
    assert(strstr(captured_response, "\"channels\":[992,") != NULL);
    assert(processor.statistics.receiver_count == 1U);

    receiver_inspection_read_result = false;
    queue_input("{\"type\":\"command\",\"request_id\":14,"
                "\"command\":\"receiver\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(receiver_inspection_read_count == 2U);
    assert(strstr(captured_response,
                  "receiver_inspection_unavailable") != NULL);
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

static void status_and_health_report_current_summary(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    static const char status[] =
        "{\"type\":\"response\",\"request_id\":10,"
        "\"command\":\"status\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"control_source\":\"NONE\","
        "\"uptime_us\":123456,"
        "\"firmware_version\":\"0.1.0\","
        "\"build_id\":\"test-build\"}\n";
    static const char health[] =
        "{\"type\":\"response\",\"request_id\":11,"
        "\"command\":\"health\",\"ok\":true,"
        "\"health\":\"WARNING\",\"state\":\"DISARMED\","
        "\"fault_data_complete\":true,\"active_fault_count\":1,"
        "\"warning_count\":1,\"fault_count\":0,\"critical_count\":0,"
        "\"dropped_fault_count\":0,\"faults\":[{\"id\":1,"
        "\"severity\":\"WARNING\",\"source\":\"USB\","
        "\"occurrence_count\":1,\"first_timestamp_us\":null,"
        "\"last_timestamp_us\":null,\"context\":null}],"
        "\"reported_fault_count\":1,\"truncated\":false}\n";

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    queue_input("{\"type\":\"command\",\"request_id\":10,"
                "\"command\":\"status\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(captured_length == sizeof(status) - 1U);
    assert(memcmp(captured_response, status, captured_length) == 0);

    assert(fault_system_report(&fault_system, 1U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    queue_input("{\"type\":\"command\",\"request_id\":11,"
                "\"command\":\"health\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(captured_length == sizeof(health) - 1U);
    assert(memcmp(captured_response, health, captured_length) == 0);
    assert(processor.statistics.status_count == 1U);
    assert(processor.statistics.health_count == 1U);
}

static void arm_and_disarm_use_the_state_machine(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    queue_input("{\"type\":\"command\",\"request_id\":20,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
    assert(active_motor_source == MOTOR_CONTROL_SOURCE_USB_TEST);
    assert(strstr(captured_response, "\"ok\":true") != NULL);
    assert(strstr(captured_response, "\"request_id\":20") != NULL);
    assert(processor.last_transition_valid);
    assert(processor.last_transition_result == SYSTEM_STATE_TRANSITION_OK);
    assert(logging_queue_count() == 1U);

    queue_input("{\"type\":\"command\",\"request_id\":21,"
                "\"command\":\"disarm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(active_motor_source == MOTOR_CONTROL_SOURCE_NONE);
    assert(processor.statistics.transition_accepted_count == 2U);
}

static void illegal_transition_is_rejected_without_state_mutation(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    queue_input("{\"type\":\"command\",\"request_id\":30,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_BOOT);
    assert(strstr(captured_response, "transition_rejected") != NULL);
    assert(processor.statistics.transition_rejected_count == 1U);
}

static void unknown_health_rejects_arm_before_the_state_machine(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    fault_system.dropped_record_count = 1U;
    motor_arm_result = MOTOR_CONTROL_ARM_BLOCKED_HEALTH;

    queue_input("{\"type\":\"command\",\"request_id\":31,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(strstr(captured_response, "health_rejected") != NULL);
    assert(processor.last_transition_valid);
    assert(processor.last_transition_result ==
           SYSTEM_STATE_TRANSITION_REJECTED);
    assert(processor.statistics.transition_rejected_count == 1U);
    assert(logging_queue_count() == 0U);

    fault_system.dropped_record_count = 0U;
    motor_arm_result = MOTOR_CONTROL_ARM_ACCEPTED;
    queue_input("{\"type\":\"command\",\"request_id\":32,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
}

static void motor_preparation_rejects_arm_before_the_state_machine(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    motor_ready_for_arm = false;
    motor_arm_result = MOTOR_CONTROL_ARM_BLOCKED_PREPARATION;

    queue_input("{\"type\":\"command\",\"request_id\":33,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(strstr(captured_response, "motor_not_ready") != NULL);
    assert(processor.last_transition_result ==
           SYSTEM_STATE_TRANSITION_REJECTED);

    motor_ready_for_arm = true;
    motor_arm_result = MOTOR_CONTROL_ARM_ACCEPTED;
    queue_input("{\"type\":\"command\",\"request_id\":34,"
                "\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
}

static void motor_test_is_bounded_and_uses_the_motor_gate(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    assert(motor_control_arm(MOTOR_CONTROL_SOURCE_USB_TEST) ==
           MOTOR_CONTROL_ARM_ACCEPTED);

    queue_input("{\"type\":\"command\",\"request_id\":33,"
                "\"command\":\"motor_test\",\"motor\":4,"
                "\"throttle\":0.02}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(motor_submit_count == 1U);
    assert(captured_motor_command.valid);
    assert(captured_motor_command.timestamp_us == current_time_us);
    assert(captured_motor_command.throttle[0] == 0.0f);
    assert(captured_motor_command.throttle[1] == 0.0f);
    assert(captured_motor_command.throttle[2] == 0.0f);
    assert(captured_motor_command.throttle[3] > 0.0199f);
    assert(captured_motor_command.throttle[3] < 0.0201f);
    assert(strstr(captured_response, "\"ok\":true") != NULL);
    assert(strstr(captured_response, "\"throttle\":0.020000") != NULL);

    active_motor_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    queue_input("{\"type\":\"command\",\"request_id\":37,"
                "\"command\":\"motor_test\",\"motor\":1,"
                "\"throttle\":0.02}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(motor_submit_count == 1U);
    assert(strstr(captured_response, "control_source_rejected") != NULL);
    active_motor_source = MOTOR_CONTROL_SOURCE_USB_TEST;

    queue_input("{\"type\":\"command\",\"request_id\":34,"
                "\"command\":\"motor_test\",\"motor\":0,"
                "\"throttle\":0.02}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(motor_submit_count == 1U);
    assert(strstr(captured_response, "motor_not_allowed") != NULL);

    queue_input("{\"type\":\"command\",\"request_id\":35,"
                "\"command\":\"motor_test\",\"motor\":1,"
                "\"throttle\":1}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(motor_submit_count == 2U);
    assert(captured_motor_command.throttle[0] == 1.0f);
    assert(strstr(captured_response, "\"ok\":true") != NULL);

    motor_submit_result = MOTOR_CONTROL_SUBMIT_INVALID_COMMAND;
    queue_input("{\"type\":\"command\",\"request_id\":36,"
                "\"command\":\"motor_test\",\"motor\":4,"
                "\"throttle\":0}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(motor_submit_count == 3U);
    assert(strstr(captured_response,
                  "\"error\":\"motor_output_error\"") != NULL);
    assert(processor.statistics.motor_test_count == 5U);
    assert(processor.statistics.motor_test_accepted_count == 2U);
    assert(processor.statistics.motor_test_rejected_count == 3U);
}

static void invalid_unsupported_and_busy_responses_are_bounded(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    queue_input("not-json");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(strstr(captured_response, "invalid_request") != NULL);
    assert(strstr(captured_response, "\"request_id\":null") != NULL);
    queue_input("{\"type\":\"command\",\"request_id\":41,"
                "\"command\":\"future\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(strstr(captured_response, "unsupported_command") != NULL);
    assert(strstr(captured_response, "\"request_id\":41") != NULL);

    write_result = USB_CDC_WRITE_BUSY;
    queue_input("{\"type\":\"command\",\"request_id\":42,"
                "\"command\":\"status\"}");
    queue_input("{\"type\":\"command\",\"request_id\":43,"
                "\"command\":\"health\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_PENDING);
    assert(input_count == 1U);
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_PENDING);
    assert(input_count == 1U);
    write_result = USB_CDC_WRITE_ACCEPTED;
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(input_count == 1U);
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(input_count == 0U);
    assert(processor.statistics.malformed_count == 1U);
    assert(processor.statistics.unsupported_count == 1U);
    assert(processor.statistics.response_busy_count == 2U);
}

static void initialization_and_invalid_state_are_checked(void)
{
    usb_command_processor_t processor = {0};
    system_state_machine_t state_machine;
    fault_system_t fault_system = {0};
    const receiver_inspection_provider_t receiver_provider = {
        .read = fake_receiver_inspection_read,
        .context = &receiver_inspection,
    };

    reset_fakes();
    system_state_machine_initialize(&state_machine);
    assert(usb_command_processor_initialize(NULL, &state_machine,
                                            &fault_system,
                                            fake_clock,
                                            &receiver_provider,
                                            "0.1.0", "test-build") ==
           USB_COMMAND_INIT_INVALID_ARGUMENT);
    assert(usb_command_processor_initialize(&processor, &state_machine,
                                            &fault_system,
                                            fake_clock,
                                            &receiver_provider,
                                            NULL, "test-build") ==
           USB_COMMAND_INIT_INVALID_ARGUMENT);
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_INVALID_STATE);
}

int main(void)
{
    status_and_health_report_current_summary();
    receiver_inspection_is_read_only_when_requested();
    arm_and_disarm_use_the_state_machine();
    illegal_transition_is_rejected_without_state_mutation();
    unknown_health_rejects_arm_before_the_state_machine();
    motor_preparation_rejects_arm_before_the_state_machine();
    motor_test_is_bounded_and_uses_the_motor_gate();
    invalid_unsupported_and_busy_responses_are_bounded();
    initialization_and_invalid_state_are_checked();
    return 0;
}
