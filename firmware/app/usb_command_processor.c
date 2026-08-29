#include "usb_command_processor.h"

#include "logging.h"
#include "usb_json_protocol.h"

#include <limits.h>
#include <stddef.h>

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static usb_command_process_result_t try_send_pending_response(
    usb_command_processor_t *processor)
{
    usb_cdc_write_result_t write_result;

    write_result = usb_cdc_transport_try_write(
        (const uint8_t *)processor->pending_response,
        processor->pending_response_length);
    switch (write_result) {
    case USB_CDC_WRITE_ACCEPTED:
        processor->pending_response_valid = false;
        processor->pending_response_length = 0U;
        saturating_increment(&processor->statistics.response_sent_count);
        return USB_COMMAND_PROCESS_RESPONSE_SENT;
    case USB_CDC_WRITE_BUSY:
        saturating_increment(&processor->statistics.response_busy_count);
        return USB_COMMAND_PROCESS_RESPONSE_PENDING;
    case USB_CDC_WRITE_ERROR:
        processor->pending_response_valid = false;
        processor->pending_response_length = 0U;
        saturating_increment(&processor->statistics.response_error_count);
        return USB_COMMAND_PROCESS_TRANSPORT_ERROR;
    }

    processor->pending_response_valid = false;
    processor->pending_response_length = 0U;
    saturating_increment(&processor->statistics.response_error_count);
    return USB_COMMAND_PROCESS_TRANSPORT_ERROR;
}

static bool build_error(usb_command_processor_t *processor,
                        const char *error)
{
    return usb_json_build_error_response(
        error,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static bool build_command_response(usb_command_processor_t *processor,
                                   usb_json_command_t command)
{
    switch (command) {
    case USB_JSON_COMMAND_STATUS:
        saturating_increment(&processor->statistics.status_count);
        return usb_json_build_status_response(
            system_state_name(processor->state_machine->current),
            processor->clock(),
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    case USB_JSON_COMMAND_HEALTH:
        saturating_increment(&processor->statistics.health_count);
        return usb_json_build_health_response(
            system_state_name(processor->state_machine->current),
            fault_system_active_count(processor->fault_system),
            processor->fault_system->dropped_record_count,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    case USB_JSON_COMMAND_ARM:
    case USB_JSON_COMMAND_DISARM: {
        const system_state_t previous = processor->state_machine->current;
        const system_state_event_t event =
            command == USB_JSON_COMMAND_ARM
                ? SYSTEM_STATE_EVENT_ARM_REQUESTED
                : SYSTEM_STATE_EVENT_DISARM_REQUESTED;

        processor->last_transition_result =
            system_state_machine_handle_event(processor->state_machine, event);
        processor->last_transition_valid = true;

        if (processor->last_transition_result == SYSTEM_STATE_TRANSITION_OK) {
            saturating_increment(
                &processor->statistics.transition_accepted_count);
            LOG_INFO(LOG_MODULE_STATE,
                     "%s -> %s source=usb",
                     system_state_name(previous),
                     system_state_name(processor->state_machine->current));
            return usb_json_build_transition_response(
                command,
                true,
                system_state_name(processor->state_machine->current),
                NULL,
                processor->pending_response,
                sizeof(processor->pending_response),
                &processor->pending_response_length);
        }

        saturating_increment(&processor->statistics.transition_rejected_count);
        return usb_json_build_transition_response(
            command,
            false,
            system_state_name(processor->state_machine->current),
            "transition_rejected",
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_UNSUPPORTED:
    case USB_JSON_COMMAND_INVALID:
        break;
    }

    return false;
}

usb_command_init_result_t usb_command_processor_initialize(
    usb_command_processor_t *processor,
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    usb_command_clock_t clock)
{
    if ((processor == NULL) || (state_machine == NULL) ||
        !state_machine->initialized || (fault_system == NULL) ||
        !fault_system->initialized || (clock == NULL)) {
        return USB_COMMAND_INIT_INVALID_ARGUMENT;
    }

    *processor = (usb_command_processor_t){
        .state_machine = state_machine,
        .fault_system = fault_system,
        .clock = clock,
        .initialized = true,
    };
    return USB_COMMAND_INIT_OK;
}

usb_command_process_result_t usb_command_processor_process_once(
    usb_command_processor_t *processor)
{
    uint8_t line[USB_CDC_RECEIVE_LINE_CAPACITY + 1U];
    size_t line_length;
    usb_cdc_line_result_t line_result;
    usb_json_request_t request;
    bool response_built;

    if ((processor == NULL) || !processor->initialized ||
        (processor->state_machine == NULL) ||
        !processor->state_machine->initialized ||
        (processor->fault_system == NULL) ||
        !processor->fault_system->initialized || (processor->clock == NULL)) {
        return USB_COMMAND_PROCESS_INVALID_STATE;
    }

    processor->last_transition_valid = false;

    if (processor->pending_response_valid) {
        return try_send_pending_response(processor);
    }

    line_result = usb_cdc_transport_read_line(line, sizeof(line), &line_length);
    if (line_result == USB_CDC_LINE_UNAVAILABLE) {
        return USB_COMMAND_PROCESS_IDLE;
    }
    if (line_result != USB_CDC_LINE_AVAILABLE) {
        return USB_COMMAND_PROCESS_INVALID_STATE;
    }

    saturating_increment(&processor->statistics.command_count);
    if (!usb_json_parse_request((const char *)line, line_length, &request)) {
        saturating_increment(&processor->statistics.malformed_count);
        response_built = build_error(processor, "invalid_request");
    } else if (request.command == USB_JSON_COMMAND_UNSUPPORTED) {
        saturating_increment(&processor->statistics.unsupported_count);
        response_built = build_error(processor, "unsupported_command");
    } else {
        response_built = build_command_response(processor, request.command);
    }

    if (!response_built) {
        processor->pending_response_length = 0U;
        saturating_increment(
            &processor->statistics.response_build_error_count);
        return USB_COMMAND_PROCESS_BUILD_ERROR;
    }

    processor->pending_response_valid = true;
    return try_send_pending_response(processor);
}
