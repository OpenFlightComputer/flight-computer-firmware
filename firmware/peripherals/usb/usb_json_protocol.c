#include "usb_json_protocol.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <stdio.h>
#include <string.h>

#define USB_JSON_TOKEN_CAPACITY 8U

static bool token_equals(const char *line,
                         const jsmntok_t *token,
                         const char *value)
{
    const size_t value_length = strlen(value);
    const size_t token_length = (size_t)(token->end - token->start);

    return (token->type == JSMN_STRING) &&
           (token_length == value_length) &&
           (memcmp(&line[token->start], value, value_length) == 0);
}

static bool finish_response(int written,
                            size_t capacity,
                            size_t *length)
{
    if ((written < 0) || ((size_t)written >= capacity)) {
        if (length != NULL) {
            *length = 0U;
        }
        return false;
    }

    *length = (size_t)written;
    return true;
}

bool usb_json_parse_request(const char *line,
                            size_t line_length,
                            usb_json_request_t *request)
{
    jsmn_parser parser;
    jsmntok_t tokens[USB_JSON_TOKEN_CAPACITY];
    bool type_seen = false;
    bool command_seen = false;
    int token_count;
    int index;

    if ((line == NULL) || (request == NULL) || (line_length == 0U)) {
        return false;
    }

    request->command = USB_JSON_COMMAND_INVALID;
    jsmn_init(&parser);
    token_count = jsmn_parse(&parser,
                             line,
                             line_length,
                             tokens,
                             USB_JSON_TOKEN_CAPACITY);
    if ((token_count != 5) || (tokens[0].type != JSMN_OBJECT)) {
        return false;
    }

    for (index = 1; index < token_count; index += 2) {
        const jsmntok_t *key = &tokens[index];
        const jsmntok_t *value = &tokens[index + 1];

        if (token_equals(line, key, "type")) {
            if (type_seen || !token_equals(line, value, "command")) {
                return false;
            }
            type_seen = true;
        } else if (token_equals(line, key, "command")) {
            if (command_seen || (value->type != JSMN_STRING)) {
                return false;
            }

            if (token_equals(line, value, "status")) {
                request->command = USB_JSON_COMMAND_STATUS;
            } else if (token_equals(line, value, "health")) {
                request->command = USB_JSON_COMMAND_HEALTH;
            } else if (token_equals(line, value, "arm")) {
                request->command = USB_JSON_COMMAND_ARM;
            } else if (token_equals(line, value, "disarm")) {
                request->command = USB_JSON_COMMAND_DISARM;
            } else {
                request->command = USB_JSON_COMMAND_UNSUPPORTED;
            }
            command_seen = true;
        } else {
            return false;
        }
    }

    return type_seen && command_seen;
}

const char *usb_json_command_name(usb_json_command_t command)
{
    switch (command) {
    case USB_JSON_COMMAND_STATUS:
        return "status";
    case USB_JSON_COMMAND_HEALTH:
        return "health";
    case USB_JSON_COMMAND_ARM:
        return "arm";
    case USB_JSON_COMMAND_DISARM:
        return "disarm";
    case USB_JSON_COMMAND_UNSUPPORTED:
        return "unsupported";
    case USB_JSON_COMMAND_INVALID:
        break;
    }

    return "invalid";
}

bool usb_json_build_error_response(const char *error,
                                   char *destination,
                                   size_t capacity,
                                   size_t *length)
{
    int written;

    if ((error == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL)) {
        return false;
    }

    written = snprintf(destination,
                       capacity,
                       "{\"type\":\"error\",\"error\":\"%s\"}\n",
                       error);
    return finish_response(written, capacity, length);
}

bool usb_json_build_transition_response(usb_json_command_t command,
                                        bool accepted,
                                        const char *state,
                                        const char *error,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length)
{
    int written;

    if (((command != USB_JSON_COMMAND_ARM) &&
         (command != USB_JSON_COMMAND_DISARM)) ||
        (state == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL) || (!accepted && (error == NULL))) {
        return false;
    }

    if (accepted) {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"command\":\"%s\","
                           "\"ok\":true,\"state\":\"%s\"}\n",
                           usb_json_command_name(command),
                           state);
    } else {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"command\":\"%s\","
                           "\"ok\":false,\"state\":\"%s\","
                           "\"error\":\"%s\"}\n",
                           usb_json_command_name(command),
                           state,
                           error);
    }

    return finish_response(written, capacity, length);
}

bool usb_json_build_status_response(const char *state,
                                    uint64_t uptime_us,
                                    char *destination,
                                    size_t capacity,
                                    size_t *length)
{
    int written;

    if ((state == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL)) {
        return false;
    }

    written = snprintf(destination,
                       capacity,
                       "{\"type\":\"response\",\"command\":\"status\","
                       "\"ok\":true,\"state\":\"%s\","
                       "\"uptime_us\":%llu}\n",
                       state,
                       (unsigned long long)uptime_us);
    return finish_response(written, capacity, length);
}
