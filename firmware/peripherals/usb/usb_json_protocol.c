#include "usb_json_protocol.h"

#include "uint64_decimal.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <stdio.h>
#include <string.h>

#define USB_JSON_TOKEN_CAPACITY 256U
#define USB_JSON_THROTTLE_SCALE 1000000U

static bool token_equals(const char *line,
                         const jsmntok_t *token,
                         const char *value)
{
    if ((line == NULL) || (token == NULL) || (value == NULL)) {
        return false;
    }
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

static bool parse_uint64(const char *line,
                         const jsmntok_t *token,
                         uint64_t *value)
{
    uint64_t parsed = 0U;
    int index;

    if ((token->type != JSMN_PRIMITIVE) ||
        (token->start >= token->end) ||
        (((token->end - token->start) > 1) &&
         (line[token->start] == '0'))) {
        return false;
    }
    for (index = token->start; index < token->end; index++) {
        const char character = line[index];
        const uint64_t digit = (uint64_t)(character - '0');

        if ((character < '0') || (character > '9') ||
            (parsed > ((UINT64_MAX - digit) / UINT64_C(10)))) {
            return false;
        }
        parsed = (parsed * UINT64_C(10)) + digit;
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

static bool parse_signed_millionths(const char *line,
                                    const jsmntok_t *token,
                                    int32_t *value)
{
    jsmntok_t magnitude = *token;
    uint32_t parsed;
    bool negative = false;

    if ((magnitude.type != JSMN_PRIMITIVE) ||
        (magnitude.start >= magnitude.end)) {
        return false;
    }
    if (line[magnitude.start] == '-') {
        negative = true;
        magnitude.start++;
    }
    if (!parse_normalized_millionths(line, &magnitude, &parsed)) {
        return false;
    }
    *value = negative ? -(int32_t)parsed : (int32_t)parsed;
    return true;
}

static bool parse_positive_millionths(const char *line,
                                      const jsmntok_t *token,
                                      uint32_t *value)
{
    uint32_t whole = 0U;
    uint32_t fraction = 0U;
    uint32_t fraction_digits = 0U;
    int index;

    if ((token == NULL) || (value == NULL) ||
        (token->type != JSMN_PRIMITIVE) ||
        (token->start >= token->end) ||
        (((token->end - token->start) > 1) &&
         (line[token->start] == '0') &&
         (line[token->start + 1] != '.'))) {
        return false;
    }
    index = token->start;
    while ((index < token->end) && (line[index] != '.')) {
        const char character = line[index];
        const uint32_t digit = (uint32_t)(character - '0');

        if ((character < '0') || (character > '9') ||
            (whole > ((UINT32_MAX - digit) / 10U))) {
            return false;
        }
        whole = (whole * 10U) + digit;
        index++;
    }
    if (index < token->end) {
        index++;
        if (index == token->end) {
            return false;
        }
        for (; index < token->end; index++) {
            const char character = line[index];

            if ((character < '0') || (character > '9') ||
                (fraction_digits >= 6U)) {
                return false;
            }
            fraction = (fraction * 10U) +
                       (uint32_t)(character - '0');
            fraction_digits++;
        }
    }
    while (fraction_digits < 6U) {
        fraction *= 10U;
        fraction_digits++;
    }
    if (whole > ((UINT32_MAX - fraction) / USB_JSON_THROTTLE_SCALE)) {
        return false;
    }
    *value = (whole * USB_JSON_THROTTLE_SCALE) + fraction;
    return true;
}

static const jsmntok_t *object_member(const char *line,
                                      const jsmntok_t *tokens,
                                      int token_count,
                                      int object_index,
                                      const char *name)
{
    bool key_position = true;
    int index;

    if ((object_index < 0) || (object_index >= token_count) ||
        (tokens[object_index].type != JSMN_OBJECT)) {
        return NULL;
    }
    for (index = object_index + 1; index + 1 < token_count; index++) {
        if (tokens[index].parent != object_index) {
            continue;
        }
        if (key_position && token_equals(line, &tokens[index], name) &&
            (tokens[index + 1].parent == object_index)) {
            return &tokens[index + 1];
        }
        key_position = !key_position;
    }
    return NULL;
}

static int token_index(const jsmntok_t *tokens, const jsmntok_t *token)
{
    return (int)(token - tokens);
}

static bool parse_direction_array(const char *line,
                                  const jsmntok_t *tokens,
                                  int token_count,
                                  const jsmntok_t *array,
                                  uint8_t directions[4])
{
    int array_index;
    int index;
    size_t direction = 0U;

    if ((array == NULL) || (array->type != JSMN_ARRAY) ||
        (array->size != 4)) {
        return false;
    }
    array_index = token_index(tokens, array);
    for (index = array_index + 1; index < token_count; index++) {
        if (tokens[index].parent != array_index) {
            continue;
        }
        if (direction >= 4U) {
            return false;
        }
        if (token_equals(line, &tokens[index], "NORMAL")) {
            directions[direction] = 0U;
        } else if (token_equals(line, &tokens[index], "REVERSED")) {
            directions[direction] = 1U;
        } else {
            return false;
        }
        direction++;
    }
    return direction == 4U;
}

static bool parse_curve(const char *line,
                        const jsmntok_t *tokens,
                        int token_count,
                        const jsmntok_t *object,
                        size_t curve_index,
                        usb_json_configuration_t *configuration)
{
    const jsmntok_t *points;
    int points_index;
    int index;
    size_t point = 0U;

    if ((object == NULL) || (object->type != JSMN_OBJECT) ||
        (object->size != 6) ||
        !token_equals(line,
                      object_member(line, tokens, token_count,
                                    token_index(tokens, object), "type"),
                      "CONTROL_POINTS") ||
        !token_equals(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, object), "interpolation"),
            "LINEAR")) {
        return false;
    }
    points = object_member(line, tokens, token_count,
                           token_index(tokens, object), "points");
    if ((points == NULL) || (points->type != JSMN_ARRAY) ||
        (points->size < 2) ||
        (points->size > (int)USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS)) {
        return false;
    }
    points_index = token_index(tokens, points);
    for (index = points_index + 1; index < token_count; index++) {
        const jsmntok_t *pair = &tokens[index];
        uint32_t values[2];
        size_t value = 0U;
        int child;

        if (pair->parent != points_index) {
            continue;
        }
        if ((pair->type != JSMN_ARRAY) || (pair->size != 2) ||
            (point >= USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS)) {
            return false;
        }
        for (child = index + 1; child < token_count; child++) {
            if (tokens[child].parent != index) {
                continue;
            }
            if ((value >= 2U) ||
                !parse_normalized_millionths(line, &tokens[child],
                                             &values[value])) {
                return false;
            }
            value++;
        }
        if (value != 2U) {
            return false;
        }
        configuration->curve_point_millionths[curve_index][point][0] =
            values[0];
        configuration->curve_point_millionths[curve_index][point][1] =
            values[1];
        point++;
    }
    configuration->curve_type[curve_index] = 0U;
    configuration->curve_interpolation[curve_index] = 0U;
    configuration->curve_point_count[curve_index] = (uint8_t)point;
    return point == (size_t)points->size;
}

static bool parse_control_configuration(
    const char *line,
    const jsmntok_t *tokens,
    int token_count,
    const jsmntok_t *control,
    usb_json_configuration_t *configuration)
{
    static const char *const axis_names[3] = {"roll", "pitch", "yaw"};
    size_t axis;

    if ((control == NULL) || (control->type != JSMN_OBJECT) ||
        (control->size != 8)) {
        return false;
    }
    for (axis = 0U; axis < 3U; axis++) {
        const jsmntok_t *axis_object = object_member(
            line, tokens, token_count, token_index(tokens, control),
            axis_names[axis]);

        if ((axis_object == NULL) || (axis_object->type != JSMN_OBJECT) ||
            (axis_object->size != 8) ||
            !parse_normalized_millionths(
                line,
                object_member(line, tokens, token_count,
                              token_index(tokens, axis_object), "deadband"),
                &configuration->control_axis_millionths[axis][0]) ||
            !parse_positive_millionths(
                line,
                object_member(line, tokens, token_count,
                              token_index(tokens, axis_object),
                              "maximum_angle_degrees"),
                &configuration->control_axis_millionths[axis][1]) ||
            !parse_positive_millionths(
                line,
                object_member(line, tokens, token_count,
                              token_index(tokens, axis_object),
                              "maximum_rate_dps"),
                &configuration->control_axis_millionths[axis][2]) ||
            !parse_curve(
                line, tokens, token_count,
                object_member(line, tokens, token_count,
                              token_index(tokens, axis_object), "curve"),
                axis, configuration)) {
            return false;
        }
    }
    {
        const jsmntok_t *throttle = object_member(
            line, tokens, token_count, token_index(tokens, control),
            "throttle");

        return (throttle != NULL) && (throttle->type == JSMN_OBJECT) &&
               (throttle->size == 6) &&
               parse_normalized_millionths(
                   line,
                   object_member(line, tokens, token_count,
                                 token_index(tokens, throttle),
                                 "zero_deadband"),
                   &configuration->throttle_millionths[0]) &&
               parse_normalized_millionths(
                   line,
                   object_member(line, tokens, token_count,
                                 token_index(tokens, throttle), "maximum"),
                   &configuration->throttle_millionths[1]) &&
               parse_curve(
                   line, tokens, token_count,
                   object_member(line, tokens, token_count,
                                 token_index(tokens, throttle), "curve"),
                   3U, configuration);
    }
}

static bool parse_configuration(const char *line,
                                const jsmntok_t *tokens,
                                int token_count,
                                const jsmntok_t *object,
                                usb_json_configuration_t *configuration)
{
    static const char *const timing_names[5] = {
        "stale_after_us", "loss_detected_after_us", "hold_last_until_us",
        "stage_two_after_us", "recovery_stable_us",
    };
    static const char *const control_names[5] = {
        "stage_one_roll", "stage_one_pitch", "stage_one_yaw",
        "stage_one_throttle", "recovery_throttle_maximum",
    };
    static const char *const mixer_names[3] = {
        "roll_factor", "pitch_factor", "yaw_factor",
    };
    const jsmntok_t *schema;
    const jsmntok_t *motors;
    const jsmntok_t *mixer;
    const jsmntok_t *failsafe;
    const jsmntok_t *imu;
    const jsmntok_t *control;
    const jsmntok_t *gyro_calibration;
    const jsmntok_t *gyro_filter;
    const jsmntok_t *attitude_estimator;
    const jsmntok_t *layout;
    const jsmntok_t *directions;
    uint32_t schema_value;
    size_t index;

    if ((object == NULL) || (object->type != JSMN_OBJECT) ||
        (object->size != 12)) {
        return false;
    }
    schema = object_member(line, tokens, token_count,
                           token_index(tokens, object), "schema_version");
    motors = object_member(line, tokens, token_count,
                           token_index(tokens, object), "motors");
    mixer = object_member(line, tokens, token_count,
                          token_index(tokens, object), "mixer");
    failsafe = object_member(line, tokens, token_count,
                             token_index(tokens, object),
                             "receiver_failsafe");
    imu = object_member(line, tokens, token_count,
                        token_index(tokens, object), "imu");
    control = object_member(line, tokens, token_count,
                            token_index(tokens, object), "control");
    if ((schema == NULL) || !parse_uint32(line, schema, &schema_value) ||
        (schema_value != 4U) || (motors == NULL) ||
        (motors->type != JSMN_OBJECT) || (motors->size != 4) ||
        (mixer == NULL) || (mixer->type != JSMN_OBJECT) ||
        (mixer->size != 6) || (failsafe == NULL) ||
        (failsafe->type != JSMN_OBJECT) || (failsafe->size != 20) ||
        (imu == NULL) || (imu->type != JSMN_OBJECT) || (imu->size != 6) ||
        !parse_control_configuration(line, tokens, token_count, control,
                                     configuration)) {
        return false;
    }
    configuration->schema_version = schema_value;

    layout = object_member(line, tokens, token_count,
                           token_index(tokens, motors), "propeller_layout");
    directions = object_member(line, tokens, token_count,
                               token_index(tokens, motors), "directions");
    if (token_equals(line, layout, "PROPS_IN")) {
        configuration->propeller_layout = 0U;
    } else if (token_equals(line, layout, "PROPS_OUT")) {
        configuration->propeller_layout = 1U;
    } else {
        return false;
    }
    if (!parse_direction_array(line, tokens, token_count, directions,
                               configuration->directions)) {
        return false;
    }

    for (index = 0U; index < 3U; index++) {
        const jsmntok_t *value = object_member(
            line, tokens, token_count, token_index(tokens, mixer),
            mixer_names[index]);
        if ((value == NULL) || !parse_normalized_millionths(
                                   line, value,
                                   &configuration->mixer_factor_millionths[index])) {
            return false;
        }
    }
    for (index = 0U; index < 5U; index++) {
        const jsmntok_t *timing = object_member(
            line, tokens, token_count, token_index(tokens, failsafe),
            timing_names[index]);
        const jsmntok_t *control = object_member(
            line, tokens, token_count, token_index(tokens, failsafe),
            control_names[index]);
        if ((timing == NULL) || !parse_uint64(
                                    line, timing,
                                    &configuration->timing_us[index]) ||
            (control == NULL) || !parse_signed_millionths(
                                     line, control,
                                     &configuration->failsafe_control_millionths[index])) {
            return false;
        }
    }
    gyro_calibration = object_member(line, tokens, token_count,
                                     token_index(tokens, imu),
                                     "gyro_calibration");
    if ((gyro_calibration == NULL) ||
        (gyro_calibration->type != JSMN_OBJECT) ||
        (gyro_calibration->size != 8) ||
        !parse_uint64(line,
                      object_member(line, tokens, token_count,
                                    token_index(tokens, gyro_calibration),
                                    "settling_duration_us"),
                      &configuration->gyro_timing_us[0]) ||
        !parse_uint64(line,
                      object_member(line, tokens, token_count,
                                    token_index(tokens, gyro_calibration),
                                    "sample_duration_us"),
                      &configuration->gyro_timing_us[1]) ||
        !parse_positive_millionths(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, gyro_calibration),
                          "maximum_rate_dps"),
            &configuration->gyro_threshold_millionths[0]) ||
        !parse_positive_millionths(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, gyro_calibration),
                          "maximum_standard_deviation_dps"),
            &configuration->gyro_threshold_millionths[1])) {
        return false;
    }
    gyro_filter = object_member(line, tokens, token_count,
                                token_index(tokens, imu), "gyro_filter");
    attitude_estimator = object_member(line, tokens, token_count,
                                       token_index(tokens, imu),
                                       "attitude_estimator");
    if ((gyro_filter == NULL) || (gyro_filter->type != JSMN_OBJECT) ||
        (gyro_filter->size != 4) ||
        !token_equals(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, gyro_filter), "type"),
            "FIRST_ORDER_LOW_PASS") ||
        !parse_positive_millionths(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, gyro_filter), "cutoff_hz"),
            &configuration->gyro_filter_cutoff_millionths) ||
        (attitude_estimator == NULL) ||
        (attitude_estimator->type != JSMN_OBJECT) ||
        (attitude_estimator->size != 6) ||
        !token_equals(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, attitude_estimator), "type"),
            "COMPLEMENTARY") ||
        !parse_positive_millionths(
            line,
            object_member(
                line, tokens, token_count,
                token_index(tokens, attitude_estimator),
                "accelerometer_correction_time_constant_s"),
            &configuration
                 ->accelerometer_correction_time_constant_millionths) ||
        !parse_uint32(
            line,
            object_member(line, tokens, token_count,
                          token_index(tokens, attitude_estimator),
                          "maximum_gap_us"),
            &configuration->attitude_maximum_gap_us)) {
        return false;
    }
    configuration->gyro_filter_type = 0U;
    configuration->attitude_estimator_type = 0U;
    return (configuration->failsafe_control_millionths[3] >= 0) &&
           (configuration->failsafe_control_millionths[4] >= 0);
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

static bool format_signed_millionths(int32_t value,
                                     char *destination,
                                     size_t capacity)
{
    const bool negative = value < 0;
    const uint32_t magnitude = negative ? (uint32_t)(-value)
                                        : (uint32_t)value;
    const int written = snprintf(destination, capacity, "%s%lu.%06lu",
                                 negative ? "-" : "",
                                 (unsigned long)(magnitude / 1000000U),
                                 (unsigned long)(magnitude % 1000000U));

    return (written >= 0) && ((size_t)written < capacity);
}

static bool format_curve(const usb_json_configuration_t *configuration,
                         size_t curve,
                         char *destination,
                         size_t capacity)
{
    size_t offset = 0U;
    size_t point;
    int written;

    if ((configuration->curve_type[curve] != 0U) ||
        (configuration->curve_interpolation[curve] != 0U) ||
        (configuration->curve_point_count[curve] < 2U) ||
        (configuration->curve_point_count[curve] >
         USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS)) {
        return false;
    }
    written = snprintf(destination, capacity,
                       "{\"type\":\"CONTROL_POINTS\","
                       "\"interpolation\":\"LINEAR\",\"points\":[");
    if ((written < 0) || ((size_t)written >= capacity)) {
        return false;
    }
    offset = (size_t)written;
    for (point = 0U; point < configuration->curve_point_count[curve];
         point++) {
        const uint32_t input =
            configuration->curve_point_millionths[curve][point][0];
        const uint32_t output =
            configuration->curve_point_millionths[curve][point][1];

        written = snprintf(
            destination + offset, capacity - offset,
            "%s[%lu.%06lu,%lu.%06lu]", point == 0U ? "" : ",",
            (unsigned long)(input / 1000000U),
            (unsigned long)(input % 1000000U),
            (unsigned long)(output / 1000000U),
            (unsigned long)(output % 1000000U));
        if ((written < 0) || ((size_t)written >= (capacity - offset))) {
            return false;
        }
        offset += (size_t)written;
    }
    written = snprintf(destination + offset, capacity - offset, "]}");
    return (written >= 0) && ((size_t)written < (capacity - offset));
}

bool usb_json_parse_request(const char *line,
                            size_t line_length,
                            usb_json_request_t *request)
{
    jsmn_parser parser;
    jsmntok_t tokens[USB_JSON_TOKEN_CAPACITY];
    const jsmntok_t *type;
    const jsmntok_t *command;
    const jsmntok_t *request_id;
    const jsmntok_t *motor;
    const jsmntok_t *throttle;
    const jsmntok_t *configuration;
    int token_count;

    if ((line == NULL) || (request == NULL) || (line_length == 0U)) {
        return false;
    }

    request->command = USB_JSON_COMMAND_INVALID;
    request->request_id = 0U;
    request->motor = 0U;
    request->throttle_millionths = 0U;
    request->configuration = (usb_json_configuration_t){0};
    jsmn_init(&parser);
    token_count = jsmn_parse(&parser,
                             line,
                             line_length,
                             tokens,
                             USB_JSON_TOKEN_CAPACITY);
    if ((token_count < 7) || (tokens[0].type != JSMN_OBJECT) ||
        (tokens[0].start != 0) ||
        (tokens[0].end != (int)line_length)) {
        return false;
    }
    type = object_member(line, tokens, token_count, 0, "type");
    command = object_member(line, tokens, token_count, 0, "command");
    request_id = object_member(line, tokens, token_count, 0, "request_id");
    motor = object_member(line, tokens, token_count, 0, "motor");
    throttle = object_member(line, tokens, token_count, 0, "throttle");
    configuration = object_member(line, tokens, token_count, 0,
                                  "configuration");
    if (!token_equals(line, type, "command") || (command == NULL) ||
        (command->type != JSMN_STRING) || (request_id == NULL) ||
        !parse_uint32(line, request_id, &request->request_id)) {
        return false;
    }

    if (token_equals(line, command, "status")) {
        request->command = USB_JSON_COMMAND_STATUS;
    } else if (token_equals(line, command, "health")) {
        request->command = USB_JSON_COMMAND_HEALTH;
    } else if (token_equals(line, command, "receiver")) {
        request->command = USB_JSON_COMMAND_RECEIVER;
    } else if (token_equals(line, command, "imu")) {
        request->command = USB_JSON_COMMAND_IMU;
    } else if (token_equals(line, command, "arm")) {
        request->command = USB_JSON_COMMAND_ARM;
    } else if (token_equals(line, command, "disarm")) {
        request->command = USB_JSON_COMMAND_DISARM;
    } else if (token_equals(line, command, "motor_test")) {
        request->command = USB_JSON_COMMAND_MOTOR_TEST;
    } else if (token_equals(line, command, "config_read")) {
        request->command = USB_JSON_COMMAND_CONFIG_READ;
    } else if (token_equals(line, command, "config_write")) {
        request->command = USB_JSON_COMMAND_CONFIG_WRITE;
    } else if (token_equals(line, command, "config_reset")) {
        request->command = USB_JSON_COMMAND_CONFIG_RESET;
    } else {
        request->command = USB_JSON_COMMAND_UNSUPPORTED;
    }

    if (request->command == USB_JSON_COMMAND_MOTOR_TEST) {
        uint32_t motor_value;
        return (tokens[0].size == 10) && (motor != NULL) &&
               parse_uint32(line, motor, &motor_value) &&
               (motor_value <= UINT8_MAX) &&
               ((request->motor = (uint8_t)motor_value), true) &&
               (throttle != NULL) && parse_normalized_millionths(
                   line, throttle, &request->throttle_millionths) &&
               (configuration == NULL);
    }
    if (request->command == USB_JSON_COMMAND_CONFIG_WRITE) {
        return (tokens[0].size == 8) && (motor == NULL) &&
               (throttle == NULL) && parse_configuration(
                   line, tokens, token_count, configuration,
                   &request->configuration);
    }
    return (tokens[0].size == 6) && (motor == NULL) &&
           (throttle == NULL) && (configuration == NULL);
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
    case USB_JSON_COMMAND_IMU:
        return "imu";
    case USB_JSON_COMMAND_ARM:
        return "arm";
    case USB_JSON_COMMAND_DISARM:
        return "disarm";
    case USB_JSON_COMMAND_MOTOR_TEST:
        return "motor_test";
    case USB_JSON_COMMAND_CONFIG_READ:
        return "config_read";
    case USB_JSON_COMMAND_CONFIG_WRITE:
        return "config_write";
    case USB_JSON_COMMAND_CONFIG_RESET:
        return "config_reset";
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

bool usb_json_build_configuration_response(
    usb_json_command_t command,
    uint32_t request_id,
    bool accepted,
    const char *configuration_source,
    const usb_json_configuration_t *configuration,
    const char *state,
    const char *error,
    char *destination,
    size_t capacity,
    size_t *length)
{
    char timing[5][UINT64_DECIMAL_BUFFER_CAPACITY];
    char controls[5][16];
    char gyro_timing[2][UINT64_DECIMAL_BUFFER_CAPACITY];
    char curves[USB_JSON_CONFIGURATION_CURVE_COUNT][300];
    size_t timing_length;
    const char *layout;
    const char *directions[4];
    size_t index;
    int written;

    if (((command != USB_JSON_COMMAND_CONFIG_READ) &&
         (command != USB_JSON_COMMAND_CONFIG_WRITE) &&
         (command != USB_JSON_COMMAND_CONFIG_RESET)) ||
        (configuration_source == NULL) || (configuration == NULL) ||
        (state == NULL) || (destination == NULL) || (capacity == 0U) ||
        (length == NULL) || (!accepted && (error == NULL)) ||
        (configuration->propeller_layout > 1U) ||
        (configuration->gyro_filter_type != 0U) ||
        (configuration->attitude_estimator_type != 0U)) {
        return false;
    }
    for (index = 0U; index < USB_JSON_CONFIGURATION_CURVE_COUNT; index++) {
        if (!format_curve(configuration, index, curves[index],
                          sizeof(curves[index]))) {
            return false;
        }
    }

    layout = configuration->propeller_layout == 0U ? "PROPS_IN"
                                                    : "PROPS_OUT";
    for (index = 0U; index < 4U; index++) {
        if (configuration->directions[index] > 1U) {
            return false;
        }
        directions[index] = configuration->directions[index] == 0U
                                ? "NORMAL"
                                : "REVERSED";
    }
    for (index = 0U; index < 5U; index++) {
        if (!uint64_decimal_format(configuration->timing_us[index], 0U,
                                   timing[index], sizeof(timing[index]),
                                   &timing_length)) {
            return false;
        }
        if (!format_signed_millionths(
                configuration->failsafe_control_millionths[index],
                controls[index], sizeof(controls[index]))) {
            return false;
        }
    }
    for (index = 0U; index < 2U; index++) {
        if (!uint64_decimal_format(configuration->gyro_timing_us[index],
                                   0U,
                                   gyro_timing[index],
                                   sizeof(gyro_timing[index]),
                                   &timing_length)) {
            return false;
        }
    }

    written = snprintf(
        destination, capacity,
        "{\"type\":\"response\",\"request_id\":%lu,"
        "\"command\":\"%s\",\"ok\":%s,\"state\":\"%s\","
        "\"source\":\"%s\",\"configuration\":{"
        "\"schema_version\":%lu,\"motors\":{"
        "\"propeller_layout\":\"%s\",\"directions\":["
        "\"%s\",\"%s\",\"%s\",\"%s\"]},\"mixer\":{"
        "\"roll_factor\":%lu.%06lu,\"pitch_factor\":%lu.%06lu,"
        "\"yaw_factor\":%lu.%06lu},\"control\":{"
        "\"roll\":{\"deadband\":%lu.%06lu,"
        "\"maximum_angle_degrees\":%lu.%06lu,"
        "\"maximum_rate_dps\":%lu.%06lu,\"curve\":%s},"
        "\"pitch\":{\"deadband\":%lu.%06lu,"
        "\"maximum_angle_degrees\":%lu.%06lu,"
        "\"maximum_rate_dps\":%lu.%06lu,\"curve\":%s},"
        "\"yaw\":{\"deadband\":%lu.%06lu,"
        "\"maximum_angle_degrees\":%lu.%06lu,"
        "\"maximum_rate_dps\":%lu.%06lu,\"curve\":%s},"
        "\"throttle\":{\"zero_deadband\":%lu.%06lu,"
        "\"maximum\":%lu.%06lu,\"curve\":%s}},"
        "\"receiver_failsafe\":{"
        "\"stale_after_us\":%s,\"loss_detected_after_us\":%s,"
        "\"hold_last_until_us\":%s,\"stage_two_after_us\":%s,"
        "\"recovery_stable_us\":%s,\"stage_one_roll\":%s,"
        "\"stage_one_pitch\":%s,\"stage_one_yaw\":%s,"
        "\"stage_one_throttle\":%s,"
        "\"recovery_throttle_maximum\":%s},\"imu\":{"
        "\"gyro_calibration\":{\"settling_duration_us\":%s,"
        "\"sample_duration_us\":%s,\"maximum_rate_dps\":%lu.%06lu,"
        "\"maximum_standard_deviation_dps\":%lu.%06lu},"
        "\"gyro_filter\":{\"type\":\"FIRST_ORDER_LOW_PASS\","
        "\"cutoff_hz\":%lu.%06lu},\"attitude_estimator\":{"
        "\"type\":\"COMPLEMENTARY\","
        "\"accelerometer_correction_time_constant_s\":%lu.%06lu,"
        "\"maximum_gap_us\":%lu}}}%s%s%s}\n",
        (unsigned long)request_id, usb_json_command_name(command),
        accepted ? "true" : "false", state, configuration_source,
        (unsigned long)configuration->schema_version, layout,
        directions[0], directions[1], directions[2], directions[3],
        (unsigned long)(configuration->mixer_factor_millionths[0] / 1000000U),
        (unsigned long)(configuration->mixer_factor_millionths[0] % 1000000U),
        (unsigned long)(configuration->mixer_factor_millionths[1] / 1000000U),
        (unsigned long)(configuration->mixer_factor_millionths[1] % 1000000U),
        (unsigned long)(configuration->mixer_factor_millionths[2] / 1000000U),
        (unsigned long)(configuration->mixer_factor_millionths[2] % 1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][0] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][0] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][1] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][1] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][2] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[0][2] %
                        1000000U), curves[0],
        (unsigned long)(configuration->control_axis_millionths[1][0] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[1][0] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[1][1] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[1][1] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[1][2] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[1][2] %
                        1000000U), curves[1],
        (unsigned long)(configuration->control_axis_millionths[2][0] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[2][0] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[2][1] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[2][1] %
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[2][2] /
                        1000000U),
        (unsigned long)(configuration->control_axis_millionths[2][2] %
                        1000000U), curves[2],
        (unsigned long)(configuration->throttle_millionths[0] / 1000000U),
        (unsigned long)(configuration->throttle_millionths[0] % 1000000U),
        (unsigned long)(configuration->throttle_millionths[1] / 1000000U),
        (unsigned long)(configuration->throttle_millionths[1] % 1000000U),
        curves[3],
        timing[0], timing[1], timing[2], timing[3], timing[4],
        controls[0], controls[1], controls[2], controls[3], controls[4],
        gyro_timing[0], gyro_timing[1],
        (unsigned long)(configuration->gyro_threshold_millionths[0] /
                        1000000U),
        (unsigned long)(configuration->gyro_threshold_millionths[0] %
                        1000000U),
        (unsigned long)(configuration->gyro_threshold_millionths[1] /
                        1000000U),
        (unsigned long)(configuration->gyro_threshold_millionths[1] %
                        1000000U),
        (unsigned long)(configuration->gyro_filter_cutoff_millionths /
                        1000000U),
        (unsigned long)(configuration->gyro_filter_cutoff_millionths %
                        1000000U),
        (unsigned long)(configuration
                            ->accelerometer_correction_time_constant_millionths /
                        1000000U),
        (unsigned long)(configuration
                            ->accelerometer_correction_time_constant_millionths %
                        1000000U),
        (unsigned long)configuration->attitude_maximum_gap_us,
        accepted ? "" : ",\"error\":\"", accepted ? "" : error,
        accepted ? "" : "\"");

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
