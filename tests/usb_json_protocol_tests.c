#include "usb_json_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static usb_json_command_t parse(const char *json)
{
    usb_json_request_t request;

    assert(usb_json_parse_request(json, strlen(json), &request));
    return request.command;
}

static void valid_commands_and_key_order_are_accepted(void)
{
    assert(parse("{\"type\":\"command\",\"command\":\"status\"}") ==
           USB_JSON_COMMAND_STATUS);
    assert(parse("{\"command\":\"health\",\"type\":\"command\"}") ==
           USB_JSON_COMMAND_HEALTH);
    assert(parse("{\"type\":\"command\",\"command\":\"arm\"}") ==
           USB_JSON_COMMAND_ARM);
    assert(parse("{\"type\":\"command\",\"command\":\"disarm\"}") ==
           USB_JSON_COMMAND_DISARM);
    assert(parse("{\"type\":\"command\",\"command\":\"future\"}") ==
           USB_JSON_COMMAND_UNSUPPORTED);
}

static void malformed_or_noncanonical_requests_are_rejected(void)
{
    static const char *invalid[] = {
        "", "[]", "{}", "{", "{\"type\":\"event\",\"command\":\"status\"}",
        "{\"type\":\"command\"}",
        "{\"type\":\"command\",\"command\":1}",
        "{\"type\":\"command\",\"command\":\"status\",\"extra\":true}",
        "{\"type\":\"command\",\"type\":\"command\"}",
        "{\"type\":\"command\",\"command\":{}}",
    };
    size_t index;

    for (index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        usb_json_request_t request;
        assert(!usb_json_parse_request(invalid[index],
                                       strlen(invalid[index]),
                                       &request));
    }
    assert(!usb_json_parse_request(NULL, 1U, NULL));
}

static void response_builders_are_exact_and_bounded(void)
{
    char output[192];
    size_t length;
    static const char status[] =
        "{\"type\":\"response\",\"command\":\"status\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"uptime_us\":42}\n";
    static const char health[] =
        "{\"type\":\"response\",\"command\":\"health\",\"ok\":true,"
        "\"state\":\"ARMED\",\"active_fault_count\":2,"
        "\"dropped_fault_count\":3}\n";
    static const char accepted[] =
        "{\"type\":\"response\",\"command\":\"arm\",\"ok\":true,"
        "\"state\":\"ARMED\"}\n";
    static const char rejected[] =
        "{\"type\":\"response\",\"command\":\"disarm\",\"ok\":false,"
        "\"state\":\"BOOT\",\"error\":\"transition_rejected\"}\n";
    static const char error[] =
        "{\"type\":\"error\",\"error\":\"invalid_request\"}\n";

    assert(usb_json_build_status_response("DISARMED", 42U, output,
                                          sizeof(output), &length));
    assert(length == sizeof(status) - 1U);
    assert(memcmp(output, status, length) == 0);
    assert(usb_json_build_health_response("ARMED", 2U, 3U, output,
                                          sizeof(output), &length));
    assert(length == sizeof(health) - 1U);
    assert(memcmp(output, health, length) == 0);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_ARM, true,
                                              "ARMED", NULL, output,
                                              sizeof(output), &length));
    assert(memcmp(output, accepted, length) == 0);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_DISARM, false,
                                              "BOOT", "transition_rejected",
                                              output, sizeof(output), &length));
    assert(memcmp(output, rejected, length) == 0);
    assert(usb_json_build_error_response("invalid_request", output,
                                         sizeof(output), &length));
    assert(memcmp(output, error, length) == 0);
    assert(!usb_json_build_error_response("invalid_request", output, 4U,
                                          &length));
    assert(length == 0U);
}

int main(void)
{
    valid_commands_and_key_order_are_accepted();
    malformed_or_noncanonical_requests_are_rejected();
    response_builders_are_exact_and_bounded();
    return 0;
}
