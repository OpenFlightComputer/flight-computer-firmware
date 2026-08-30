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

static bool parse_uint32(const char *line,
                         const jsmntok_t *token,
                         uint32_t *value)
{
    uint32_t parsed = 0U;
    int index;

    if ((token->type != JSMN_PRIMITIVE) ||
        (token->start >= token->end) ||
        (((token->end - token->start) > 1) &&
         (line[token->start] == '0'))) {
        return false;
    }

    for (index = token->start; index < token->end; index++) {
        const char character = line[index];
        const uint32_t digit = (uint32_t)(character - '0');

        if ((character < '0') || (character > '9') ||
            (parsed > ((UINT32_MAX - digit) / 10U))) {
            return false;
        }
        parsed = (parsed * 10U) + digit;
    }

    *value = parsed;
    return true;
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
    bool request_id_seen = false;
    int token_count;
    int index;

    if ((line == NULL) || (request == NULL) || (line_length == 0U)) {
        return false;
    }

    request->command = USB_JSON_COMMAND_INVALID;
    request->request_id = 0U;
    jsmn_init(&parser);
    token_count = jsmn_parse(&parser,
                             line,
                             line_length,
                             tokens,
                             USB_JSON_TOKEN_CAPACITY);
    if ((token_count != 7) || (tokens[0].type != JSMN_OBJECT)) {
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
        } else if (token_equals(line, key, "request_id")) {
            if (request_id_seen ||
                !parse_uint32(line, value, &request->request_id)) {
                return false;
            }
            request_id_seen = true;
        } else {
            return false;
        }
    }

    return type_seen && command_seen && request_id_seen;
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

bool usb_json_build_error_response(bool request_id_valid,
                                   uint32_t request_id,
                                   const char *error,
                                   char *destination,
                                   size_t capacity,
                                   size_t *length)
{
    int written;

    if ((error == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL)) {
        return false;
    }

    if (request_id_valid) {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"error\",\"request_id\":%lu,"
                           "\"error\":\"%s\"}\n",
                           (unsigned long)request_id,
                           error);
    } else {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"error\",\"request_id\":null,"
                           "\"error\":\"%s\"}\n",
                           error);
    }
    return finish_response(written, capacity, length);
}

bool usb_json_build_transition_response(usb_json_command_t command,
                                        uint32_t request_id,
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
                           "{\"type\":\"response\",\"request_id\":%lu,"
                           "\"command\":\"%s\","
                           "\"ok\":true,\"state\":\"%s\"}\n",
                           (unsigned long)request_id,
                           usb_json_command_name(command),
                           state);
    } else {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"request_id\":%lu,"
                           "\"command\":\"%s\","
                           "\"ok\":false,\"state\":\"%s\","
                           "\"error\":\"%s\"}\n",
                           (unsigned long)request_id,
                           usb_json_command_name(command),
                           state,
                           error);
    }

    return finish_response(written, capacity, length);
}

bool usb_json_build_status_response(const char *state,
                                    uint32_t request_id,
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
                       "{\"type\":\"response\",\"request_id\":%lu,"
                       "\"command\":\"status\","
                       "\"ok\":true,\"state\":\"%s\","
                       "\"uptime_us\":%llu}\n",
                       (unsigned long)request_id,
                       state,
                       (unsigned long long)uptime_us);
    return finish_response(written, capacity, length);
}
