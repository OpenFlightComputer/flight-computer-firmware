#include "usb_json_protocol.h"

#include "uint64_decimal.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <stdio.h>
#include <string.h>

#define USB_JSON_TOKEN_CAPACITY 12U
#define USB_JSON_THROTTLE_SCALE 1000000U

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

static bool parse_normalized_millionths(const char *line,
                                        const jsmntok_t *token,
                                        uint32_t *value)
{
    uint32_t whole;
    uint32_t fraction = 0U;
    uint32_t fraction_digits = 0U;
    int index;

    if ((token->type != JSMN_PRIMITIVE) ||
        (token->start >= token->end) ||
        ((line[token->start] != '0') && (line[token->start] != '1'))) {
        return false;
    }

    whole = (uint32_t)(line[token->start] - '0');
    index = token->start + 1;
    if (index == token->end) {
        *value = whole * USB_JSON_THROTTLE_SCALE;
        return true;
    }
    if ((line[index] != '.') || ((index + 1) == token->end)) {
        return false;
    }

    for (index++; index < token->end; index++) {
        const char character = line[index];

        if ((character < '0') || (character > '9') ||
            (fraction_digits >= 6U)) {
            return false;
        }
        fraction = (fraction * 10U) + (uint32_t)(character - '0');
        fraction_digits++;
    }
    while (fraction_digits < 6U) {
        fraction *= 10U;
        fraction_digits++;
    }
    if ((whole == 1U) && (fraction != 0U)) {
        return false;
    }

    *value = (whole * USB_JSON_THROTTLE_SCALE) + fraction;
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
    bool motor_seen = false;
    bool throttle_seen = false;
    bool direction_seen = false;
    int token_count;
    int index;

    if ((line == NULL) || (request == NULL) || (line_length == 0U)) {
        return false;
    }

    request->command = USB_JSON_COMMAND_INVALID;
    request->request_id = 0U;
    request->motor = 0U;
    request->throttle_millionths = 0U;
    request->direction = USB_JSON_MOTOR_DIRECTION_INVALID;
    jsmn_init(&parser);
    token_count = jsmn_parse(&parser,
                             line,
                             line_length,
                             tokens,
                             USB_JSON_TOKEN_CAPACITY);
    if (((token_count != 7) && (token_count != 11)) ||
        (tokens[0].type != JSMN_OBJECT)) {
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
            } else if (token_equals(line, value, "receiver")) {
                request->command = USB_JSON_COMMAND_RECEIVER;
            } else if (token_equals(line, value, "arm")) {
                request->command = USB_JSON_COMMAND_ARM;
            } else if (token_equals(line, value, "disarm")) {
                request->command = USB_JSON_COMMAND_DISARM;
            } else if (token_equals(line, value, "motor_test")) {
                request->command = USB_JSON_COMMAND_MOTOR_TEST;
            } else if (token_equals(line, value, "motor_direction")) {
                request->command = USB_JSON_COMMAND_MOTOR_DIRECTION;
            } else if (token_equals(line, value, "motor_direction_set")) {
                request->command = USB_JSON_COMMAND_MOTOR_DIRECTION_SET;
            } else if (token_equals(line, value,
                                    "motor_configuration_reset")) {
                request->command =
                    USB_JSON_COMMAND_MOTOR_CONFIGURATION_RESET;
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
        } else if (token_equals(line, key, "motor")) {
            uint32_t motor;

            if (motor_seen || !parse_uint32(line, value, &motor) ||
                (motor > UINT8_MAX)) {
                return false;
            }
            request->motor = (uint8_t)motor;
            motor_seen = true;
        } else if (token_equals(line, key, "throttle")) {
            if (throttle_seen ||
                !parse_normalized_millionths(
                    line, value, &request->throttle_millionths)) {
                return false;
            }
            throttle_seen = true;
        } else if (token_equals(line, key, "direction")) {
            if (direction_seen || (value->type != JSMN_STRING)) {
                return false;
            }
            if (token_equals(line, value, "NORMAL")) {
                request->direction = USB_JSON_MOTOR_DIRECTION_NORMAL;
            } else if (token_equals(line, value, "REVERSED")) {
                request->direction = USB_JSON_MOTOR_DIRECTION_REVERSED;
            } else {
                return false;
            }
            direction_seen = true;
        } else {
            return false;
        }
    }

    if (!type_seen || !command_seen || !request_id_seen) {
        return false;
    }
    if (request->command == USB_JSON_COMMAND_MOTOR_TEST) {
        return motor_seen && throttle_seen && !direction_seen &&
               (token_count == 11);
    }
    if (request->command == USB_JSON_COMMAND_MOTOR_DIRECTION_SET) {
        return motor_seen && !throttle_seen && direction_seen &&
               (token_count == 11);
    }

    return !motor_seen && !throttle_seen && !direction_seen &&
           (token_count == 7);
}

const char *usb_json_command_name(usb_json_command_t command)
{
    switch (command) {
    case USB_JSON_COMMAND_STATUS:
        return "status";
    case USB_JSON_COMMAND_HEALTH:
        return "health";
    case USB_JSON_COMMAND_RECEIVER:
        return "receiver";
    case USB_JSON_COMMAND_ARM:
        return "arm";
    case USB_JSON_COMMAND_DISARM:
        return "disarm";
    case USB_JSON_COMMAND_MOTOR_TEST:
        return "motor_test";
    case USB_JSON_COMMAND_MOTOR_DIRECTION:
        return "motor_direction";
    case USB_JSON_COMMAND_MOTOR_DIRECTION_SET:
        return "motor_direction_set";
    case USB_JSON_COMMAND_MOTOR_CONFIGURATION_RESET:
        return "motor_configuration_reset";
    case USB_JSON_COMMAND_UNSUPPORTED:
        return "unsupported";
    case USB_JSON_COMMAND_INVALID:
        break;
    }

    return "invalid";
}

bool usb_json_build_motor_test_response(uint32_t request_id,
                                        bool accepted,
                                        uint8_t motor,
                                        uint32_t throttle_millionths,
                                        const char *state,
                                        const char *error,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length)
{
    const unsigned long throttle_whole =
        (unsigned long)(throttle_millionths / USB_JSON_THROTTLE_SCALE);
    const unsigned long throttle_fraction =
        (unsigned long)(throttle_millionths % USB_JSON_THROTTLE_SCALE);
    int written;

    if ((state == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL) || (!accepted && (error == NULL)) ||
        (throttle_millionths > USB_JSON_THROTTLE_SCALE)) {
        return false;
    }

    if (accepted) {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"request_id\":%lu,"
                           "\"command\":\"motor_test\",\"ok\":true,"
                           "\"state\":\"%s\",\"motor\":%u,"
                           "\"throttle\":%lu.%06lu}\n",
                           (unsigned long)request_id,
                           state,
                           (unsigned int)motor,
                           throttle_whole,
                           throttle_fraction);
    } else {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"request_id\":%lu,"
                           "\"command\":\"motor_test\",\"ok\":false,"
                           "\"state\":\"%s\",\"motor\":%u,"
                           "\"throttle\":%lu.%06lu,\"error\":\"%s\"}\n",
                           (unsigned long)request_id,
                           state,
                           (unsigned int)motor,
                           throttle_whole,
                           throttle_fraction,
                           error);
    }

    return finish_response(written, capacity, length);
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
                                        bool pending,
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
        (length == NULL) || (!accepted && (error == NULL)) ||
        (pending && !accepted)) {
        return false;
    }

    if (accepted && pending) {
        written = snprintf(destination,
                           capacity,
                           "{\"type\":\"response\",\"request_id\":%lu,"
                           "\"command\":\"%s\","
                           "\"ok\":true,\"pending\":true,"
                           "\"state\":\"%s\"}\n",
                           (unsigned long)request_id,
                           usb_json_command_name(command),
                           state);
    } else if (accepted) {
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

bool usb_json_build_motor_configuration_response(
    usb_json_command_t command,
    uint32_t request_id,
    bool accepted,
    uint8_t motor,
    const char *selected_direction,
    const char *configuration_source,
    const char *const directions[4],
    const char *state,
    const char *error,
    char *destination,
    size_t capacity,
    size_t *length)
{
    int written;

    if (((command != USB_JSON_COMMAND_MOTOR_DIRECTION) &&
         (command != USB_JSON_COMMAND_MOTOR_DIRECTION_SET) &&
         (command != USB_JSON_COMMAND_MOTOR_CONFIGURATION_RESET)) ||
        (configuration_source == NULL) || (directions == NULL) ||
        (directions[0] == NULL) || (directions[1] == NULL) ||
        (directions[2] == NULL) || (directions[3] == NULL) ||
        (state == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL) || (!accepted && (error == NULL)) ||
        ((command == USB_JSON_COMMAND_MOTOR_DIRECTION_SET) &&
         ((motor == 0U) || (selected_direction == NULL)))) {
        return false;
    }

    if (accepted &&
        (command == USB_JSON_COMMAND_MOTOR_DIRECTION_SET)) {
        written = snprintf(
            destination,
            capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"%s\",\"ok\":true,\"state\":\"%s\","
            "\"motor\":%u,\"direction\":\"%s\",\"source\":\"%s\","
            "\"directions\":[\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
            (unsigned long)request_id,
            usb_json_command_name(command),
            state,
            (unsigned int)motor,
            selected_direction,
            configuration_source,
            directions[0], directions[1], directions[2], directions[3]);
    } else if (accepted) {
        written = snprintf(
            destination,
            capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"%s\",\"ok\":true,\"state\":\"%s\","
            "\"source\":\"%s\",\"directions\":[\"%s\",\"%s\","
            "\"%s\",\"%s\"]}\n",
            (unsigned long)request_id,
            usb_json_command_name(command),
            state,
            configuration_source,
            directions[0], directions[1], directions[2], directions[3]);
    } else {
        written = snprintf(
            destination,
            capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"%s\",\"ok\":false,\"state\":\"%s\","
            "\"source\":\"%s\",\"directions\":[\"%s\",\"%s\","
            "\"%s\",\"%s\"],\"error\":\"%s\"}\n",
            (unsigned long)request_id,
            usb_json_command_name(command),
            state,
            configuration_source,
            directions[0], directions[1], directions[2], directions[3],
            error);
    }

    return finish_response(written, capacity, length);
}

bool usb_json_build_status_response(const char *state,
                                    const char *control_source,
                                    uint32_t request_id,
                                    uint64_t uptime_us,
                                    const char *firmware_version,
                                    const char *build_id,
                                    char *destination,
                                    size_t capacity,
                                    size_t *length)
{
    char uptime[UINT64_DECIMAL_BUFFER_CAPACITY];
    size_t uptime_length;
    int written;

    if ((state == NULL) || (control_source == NULL) ||
        (firmware_version == NULL) ||
        (build_id == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL)) {
        return false;
    }

    if (!uint64_decimal_format(uptime_us,
                               0U,
                               uptime,
                               sizeof(uptime),
                               &uptime_length)) {
        *length = 0U;
        return false;
    }

    written = snprintf(destination,
                       capacity,
                       "{\"type\":\"response\",\"request_id\":%lu,"
                       "\"command\":\"status\","
                       "\"ok\":true,\"state\":\"%s\","
                       "\"control_source\":\"%s\","
                       "\"uptime_us\":%s,"
                       "\"firmware_version\":\"%s\","
                       "\"build_id\":\"%s\"}\n",
                       (unsigned long)request_id,
                       state,
                       control_source,
                       uptime,
                       firmware_version,
                       build_id);
    return finish_response(written, capacity, length);
}
