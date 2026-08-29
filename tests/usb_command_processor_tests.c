#include "fault.h"
#include "logging.h"
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
    logging_initialize();
}

static void initialize_system(usb_command_processor_t *processor,
                              system_state_machine_t *state_machine,
                              fault_system_t *fault_system)
{
    static const fault_definition_t definitions[] = {
        {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_USB},
    };

    system_state_machine_initialize(state_machine);
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   definitions,
                                   1U) == FAULT_INIT_OK);
    assert(usb_command_processor_initialize(processor,
                                            state_machine,
                                            fault_system,
                                            fake_clock) ==
           USB_COMMAND_INIT_OK);
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
        "{\"type\":\"response\",\"command\":\"status\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"uptime_us\":123456}\n";
    static const char health[] =
        "{\"type\":\"response\",\"command\":\"health\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"active_fault_count\":1,"
        "\"dropped_fault_count\":0}\n";

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    enter_disarmed(&state_machine);
    queue_input("{\"type\":\"command\",\"command\":\"status\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(captured_length == sizeof(status) - 1U);
    assert(memcmp(captured_response, status, captured_length) == 0);

    assert(fault_system_report(&fault_system, 1U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    queue_input("{\"type\":\"command\",\"command\":\"health\"}");
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
    queue_input("{\"type\":\"command\",\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_ARMED);
    assert(strstr(captured_response, "\"ok\":true") != NULL);
    assert(processor.last_transition_valid);
    assert(processor.last_transition_result == SYSTEM_STATE_TRANSITION_OK);
    assert(logging_queue_count() == 1U);

    queue_input("{\"type\":\"command\",\"command\":\"disarm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_DISARMED);
    assert(processor.statistics.transition_accepted_count == 2U);
}

static void illegal_transition_is_rejected_without_state_mutation(void)
{
    usb_command_processor_t processor;
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    reset_fakes();
    initialize_system(&processor, &state_machine, &fault_system);
    queue_input("{\"type\":\"command\",\"command\":\"arm\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(state_machine.current == SYSTEM_STATE_BOOT);
    assert(strstr(captured_response, "transition_rejected") != NULL);
    assert(processor.statistics.transition_rejected_count == 1U);
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
    queue_input("{\"type\":\"command\",\"command\":\"future\"}");
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_RESPONSE_SENT);
    assert(strstr(captured_response, "unsupported_command") != NULL);

    write_result = USB_CDC_WRITE_BUSY;
    queue_input("{\"type\":\"command\",\"command\":\"status\"}");
    queue_input("{\"type\":\"command\",\"command\":\"health\"}");
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

    reset_fakes();
    system_state_machine_initialize(&state_machine);
    assert(usb_command_processor_initialize(NULL, &state_machine,
                                            &fault_system, fake_clock) ==
           USB_COMMAND_INIT_INVALID_ARGUMENT);
    assert(usb_command_processor_process_once(&processor) ==
           USB_COMMAND_PROCESS_INVALID_STATE);
}

int main(void)
{
    status_and_health_report_current_summary();
    arm_and_disarm_use_the_state_machine();
    illegal_transition_is_rejected_without_state_mutation();
    invalid_unsupported_and_busy_responses_are_bounded();
    initialization_and_invalid_state_are_checked();
    return 0;
}
