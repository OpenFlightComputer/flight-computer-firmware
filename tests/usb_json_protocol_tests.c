#include "usb_json_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static usb_json_request_t parse(const char *json)
{
    usb_json_request_t request;

    assert(usb_json_parse_request(json, strlen(json), &request));
    return request;
}

static void valid_commands_and_key_order_are_accepted(void)
{
    usb_json_request_t request;

    request = parse("{\"type\":\"command\",\"request_id\":0,"
                    "\"command\":\"status\"}");
    assert(request.command == USB_JSON_COMMAND_STATUS);
    assert(request.request_id == 0U);
    request = parse("{\"command\":\"health\",\"type\":\"command\","
                    "\"request_id\":42}");
    assert(request.command == USB_JSON_COMMAND_HEALTH);
    assert(request.request_id == 42U);
    request = parse("{\"request_id\":4294967295,\"type\":\"command\","
                    "\"command\":\"arm\"}");
    assert(request.command == USB_JSON_COMMAND_ARM);
    assert(request.request_id == UINT32_MAX);
    assert(parse("{\"type\":\"command\",\"request_id\":3,"
                 "\"command\":\"disarm\"}").command ==
           USB_JSON_COMMAND_DISARM);
    assert(parse("{\"type\":\"command\",\"request_id\":4,"
                 "\"command\":\"future\"}").command ==
           USB_JSON_COMMAND_UNSUPPORTED);
}

static void malformed_or_noncanonical_requests_are_rejected(void)
{
    static const char *invalid[] = {
        "", "[]", "{}", "{", "{\"type\":\"event\",\"command\":\"status\"}",
        "{\"type\":\"command\"}",
        "{\"type\":\"command\",\"command\":1}",
        "{\"type\":\"command\",\"command\":\"status\",\"request_id\":1,"
        "\"extra\":true}",
        "{\"type\":\"command\",\"type\":\"command\"}",
        "{\"type\":\"command\",\"command\":{}}",
        "{\"type\":\"command\",\"command\":\"status\",\"request_id\":-1}",
        "{\"type\":\"command\",\"command\":\"status\",\"request_id\":01}",
        "{\"type\":\"command\",\"command\":\"status\","
        "\"request_id\":4294967296}",
        "{\"type\":\"command\",\"command\":\"status\","
        "\"request_id\":\"1\"}",
        "{\"type\":\"command\",\"command\":\"status\",\"request_id\":1,"
        "\"request_id\":2}",
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
        "{\"type\":\"response\",\"request_id\":42,"
        "\"command\":\"status\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"uptime_us\":42}\n";
    static const char accepted[] =
        "{\"type\":\"response\",\"request_id\":7,"
        "\"command\":\"arm\",\"ok\":true,"
        "\"state\":\"ARMED\"}\n";
    static const char rejected[] =
        "{\"type\":\"response\",\"request_id\":8,"
        "\"command\":\"disarm\",\"ok\":false,"
        "\"state\":\"BOOT\",\"error\":\"transition_rejected\"}\n";
    static const char error[] =
        "{\"type\":\"error\",\"request_id\":null,"
        "\"error\":\"invalid_request\"}\n";
    static const char correlated_error[] =
        "{\"type\":\"error\",\"request_id\":9,"
        "\"error\":\"unsupported_command\"}\n";

    assert(usb_json_build_status_response("DISARMED", 42U, 42U, output,
                                          sizeof(output), &length));
    assert(length == sizeof(status) - 1U);
    assert(memcmp(output, status, length) == 0);
    assert(usb_json_build_status_response("DISARMED",
                                          UINT32_MAX,
                                          UINT64_MAX,
                                          output,
                                          sizeof(output),
                                          &length));
    assert(strstr(output,
                  "\"request_id\":4294967295") != NULL);
    assert(strstr(output,
                  "\"uptime_us\":18446744073709551615}\n") != NULL);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_ARM, 7U, true,
                                              "ARMED", NULL, output,
                                              sizeof(output), &length));
    assert(memcmp(output, accepted, length) == 0);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_DISARM, 8U, false,
                                              "BOOT", "transition_rejected",
                                              output, sizeof(output), &length));
    assert(memcmp(output, rejected, length) == 0);
    assert(usb_json_build_error_response(false, 0U, "invalid_request", output,
                                         sizeof(output), &length));
    assert(memcmp(output, error, length) == 0);
    assert(usb_json_build_error_response(true, 9U, "unsupported_command",
                                         output, sizeof(output), &length));
    assert(memcmp(output, correlated_error, length) == 0);
    assert(!usb_json_build_error_response(false, 0U, "invalid_request",
                                          output, 4U,
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
