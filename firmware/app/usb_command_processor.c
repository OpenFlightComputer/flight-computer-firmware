#include "usb_command_processor.h"

#include "motor_safety_policy.h"
#include "health.h"
#include "logging.h"
#include "motor_control.h"
#include "usb_health_response.h"
#include "usb_json_protocol.h"

#include <limits.h>
#include <stddef.h>

#define USB_MOTOR_TEST_ALLOWED_MOTOR 1U
#define USB_MOTOR_TEST_MAX_THROTTLE_MILLIONTHS 100000U
#define USB_MOTOR_TEST_THROTTLE_SCALE 1000000.0f

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
                        bool request_id_valid,
                        uint32_t request_id,
                        const char *error)
{
    return usb_json_build_error_response(
        request_id_valid,
        request_id,
        error,
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static const char *motor_test_error(motor_control_submit_result_t result)
{
    switch (result) {
    case MOTOR_CONTROL_SUBMIT_BUSY:
        return "busy";
    case MOTOR_CONTROL_SUBMIT_BLOCKED_STATE:
        return "state_rejected";
    case MOTOR_CONTROL_SUBMIT_BLOCKED_HEALTH:
        return "health_rejected";
    case MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED:
    case MOTOR_CONTROL_SUBMIT_INVALID_COMMAND:
    case MOTOR_CONTROL_SUBMIT_STALE_COMMAND:
    case MOTOR_CONTROL_SUBMIT_MAPPING_ERROR:
    case MOTOR_CONTROL_SUBMIT_BACKEND_ERROR:
    case MOTOR_CONTROL_SUBMIT_FORCE_STOP_ERROR:
        return "motor_output_error";
    case MOTOR_CONTROL_SUBMIT_ACCEPTED:
        break;
    }

    return "motor_output_error";
}

static bool build_motor_test_response(
    usb_command_processor_t *processor,
    const usb_json_request_t *request)
{
    float throttles[MOTOR_COMMAND_MOTOR_COUNT] = {0.0f};
    motor_command_t command;
    motor_control_submit_result_t result;

    saturating_increment(&processor->statistics.motor_test_count);
    if (request->motor != USB_MOTOR_TEST_ALLOWED_MOTOR) {
        saturating_increment(&processor->statistics.motor_test_rejected_count);
        return usb_json_build_motor_test_response(
            request->request_id,
            false,
            request->motor,
            request->throttle_millionths,
            system_state_name(processor->state_machine->current),
            "motor_not_allowed",
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    if (request->throttle_millionths >
        USB_MOTOR_TEST_MAX_THROTTLE_MILLIONTHS) {
        saturating_increment(&processor->statistics.motor_test_rejected_count);
        return usb_json_build_motor_test_response(
            request->request_id,
            false,
            request->motor,
            request->throttle_millionths,
            system_state_name(processor->state_machine->current),
            "throttle_out_of_range",
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }

    throttles[request->motor - 1U] =
        (float)request->throttle_millionths /
        USB_MOTOR_TEST_THROTTLE_SCALE;
    if (motor_command_create(&command, throttles, processor->clock()) !=
        MOTOR_COMMAND_CREATE_OK) {
        result = MOTOR_CONTROL_SUBMIT_INVALID_COMMAND;
    } else {
        result = motor_control_submit(&command);
    }

    if (result == MOTOR_CONTROL_SUBMIT_ACCEPTED) {
        saturating_increment(&processor->statistics.motor_test_accepted_count);
    } else {
        saturating_increment(&processor->statistics.motor_test_rejected_count);
    }
    return usb_json_build_motor_test_response(
        request->request_id,
        result == MOTOR_CONTROL_SUBMIT_ACCEPTED,
        request->motor,
        request->throttle_millionths,
        system_state_name(processor->state_machine->current),
        result == MOTOR_CONTROL_SUBMIT_ACCEPTED ? NULL
                                               : motor_test_error(result),
        processor->pending_response,
        sizeof(processor->pending_response),
        &processor->pending_response_length);
}

static bool build_command_response(usb_command_processor_t *processor,
                                   const usb_json_request_t *request)
{
    switch (request->command) {
    case USB_JSON_COMMAND_STATUS:
        saturating_increment(&processor->statistics.status_count);
        return usb_json_build_status_response(
            system_state_name(processor->state_machine->current),
            request->request_id,
            processor->clock(),
            processor->firmware_version,
            processor->build_id,
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    case USB_JSON_COMMAND_HEALTH: {
        health_summary_t summary;

        saturating_increment(&processor->statistics.health_count);
        if (health_evaluate(processor->fault_system, &summary) !=
            HEALTH_EVALUATE_OK) {
            return false;
        }
        return usb_health_response_build(&summary,
                                         processor->fault_system,
                                         request->request_id,
                                         processor->pending_response,
                                         sizeof(processor->pending_response),
                                         &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_ARM:
    case USB_JSON_COMMAND_DISARM: {
        const system_state_t previous = processor->state_machine->current;
        const system_state_event_t event =
            request->command == USB_JSON_COMMAND_ARM
                ? SYSTEM_STATE_EVENT_ARM_REQUESTED
                : SYSTEM_STATE_EVENT_DISARM_REQUESTED;
        const bool arm_health_rejected =
            (request->command == USB_JSON_COMMAND_ARM) &&
            !motor_fault_state_allows_arm(processor->fault_system);

        if (arm_health_rejected) {
            processor->last_transition_result =
                SYSTEM_STATE_TRANSITION_REJECTED;
        } else {
            processor->last_transition_result =
                system_state_machine_handle_event(processor->state_machine,
                                                  event);
        }
        processor->last_transition_valid = true;

        if (processor->last_transition_result == SYSTEM_STATE_TRANSITION_OK) {
            saturating_increment(
                &processor->statistics.transition_accepted_count);
            LOG_INFO(LOG_MODULE_STATE,
                     "%s -> %s source=usb",
                     system_state_name(previous),
                     system_state_name(processor->state_machine->current));
            return usb_json_build_transition_response(
                request->command,
                request->request_id,
                true,
                system_state_name(processor->state_machine->current),
                NULL,
                processor->pending_response,
                sizeof(processor->pending_response),
                &processor->pending_response_length);
        }

        saturating_increment(&processor->statistics.transition_rejected_count);
        return usb_json_build_transition_response(
            request->command,
            request->request_id,
            false,
            system_state_name(processor->state_machine->current),
            arm_health_rejected ? "health_rejected"
                                : "transition_rejected",
            processor->pending_response,
            sizeof(processor->pending_response),
            &processor->pending_response_length);
    }
    case USB_JSON_COMMAND_MOTOR_TEST:
        return build_motor_test_response(processor, request);
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
    usb_command_clock_t clock,
    const char *firmware_version,
    const char *build_id)
{
    if ((processor == NULL) || (state_machine == NULL) ||
        !state_machine->initialized || (fault_system == NULL) ||
        !fault_system->initialized || (clock == NULL) ||
        (firmware_version == NULL) || (build_id == NULL)) {
        return USB_COMMAND_INIT_INVALID_ARGUMENT;
    }

    *processor = (usb_command_processor_t){
        .state_machine = state_machine,
        .fault_system = fault_system,
        .clock = clock,
        .firmware_version = firmware_version,
        .build_id = build_id,
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
        !processor->fault_system->initialized || (processor->clock == NULL) ||
        (processor->firmware_version == NULL) ||
        (processor->build_id == NULL)) {
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
        response_built = build_error(processor,
                                     false,
                                     0U,
                                     "invalid_request");
    } else if (request.command == USB_JSON_COMMAND_UNSUPPORTED) {
        saturating_increment(&processor->statistics.unsupported_count);
        response_built = build_error(processor,
                                     true,
                                     request.request_id,
                                     "unsupported_command");
    } else {
        response_built = build_command_response(processor, &request);
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
