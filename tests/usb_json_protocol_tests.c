#include "usb_json_protocol.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define RATE_CONTROLLER_JSON \
    "\"rate_controller\":{\"type\":\"PID\",\"maximum_gap_us\":10000," \
    "\"roll\":{\"kp\":0.002,\"ki\":0.001,\"kd\":0.00001," \
    "\"integral_limit\":0.15,\"output_limit\":0.30}," \
    "\"pitch\":{\"kp\":0.002,\"ki\":0.001,\"kd\":0.00001," \
    "\"integral_limit\":0.15,\"output_limit\":0.30}," \
    "\"yaw\":{\"kp\":0.0015,\"ki\":0.0008,\"kd\":0.0," \
    "\"integral_limit\":0.10,\"output_limit\":0.20}}"
#define ATTITUDE_CONTROLLER_JSON \
    "\"attitude_controller\":{\"roll_gain_per_s\":4.0," \
    "\"pitch_gain_per_s\":4.0},"

static void assert_valid_json_line(const char *line, size_t length)
{
    jsmn_parser parser;
    jsmntok_t tokens[320];
    int token_count;

    assert(length > 1U);
    assert(line[length - 1U] == '\n');
    jsmn_init(&parser);
    token_count = jsmn_parse(&parser, line, length - 1U, tokens, 320U);
    assert(token_count > 0);
    assert(tokens[0].type == JSMN_OBJECT);
    assert(tokens[0].start == 0);
    assert(tokens[0].end == (int)(length - 1U));
}

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
    assert(parse("{\"type\":\"command\",\"request_id\":43,"
                 "\"command\":\"receiver\"}").command ==
           USB_JSON_COMMAND_RECEIVER);
    assert(parse("{\"type\":\"command\",\"request_id\":44,"
                 "\"command\":\"imu\"}").command ==
           USB_JSON_COMMAND_IMU);
    request = parse("{\"type\":\"command\",\"request_id\":45,"
                    "\"command\":\"control_trace_start\","
                    "\"level\":\"HIGH_RATE\"}");
    assert(request.command == USB_JSON_COMMAND_CONTROL_TRACE_START);
    assert(request.trace_level == USB_JSON_TRACE_LEVEL_HIGH_RATE);
    assert(parse("{\"type\":\"command\",\"request_id\":46,"
                 "\"command\":\"control_trace_read\"}").command ==
           USB_JSON_COMMAND_CONTROL_TRACE_READ);
    assert(parse("{\"type\":\"command\",\"request_id\":47,"
                 "\"command\":\"control_trace_stop\"}").command ==
           USB_JSON_COMMAND_CONTROL_TRACE_STOP);
    request = parse("{\"request_id\":4294967295,\"type\":\"command\","
                    "\"command\":\"arm\"}");
    assert(request.command == USB_JSON_COMMAND_ARM);
    assert(request.request_id == UINT32_MAX);
    assert(parse("{\"type\":\"command\",\"request_id\":3,"
                 "\"command\":\"disarm\"}").command ==
           USB_JSON_COMMAND_DISARM);
    request = parse("{\"throttle\":0.02,\"motor\":1,"
                    "\"command\":\"motor_test\",\"request_id\":5,"
                    "\"type\":\"command\"}");
    assert(request.command == USB_JSON_COMMAND_MOTOR_TEST);
    assert(request.request_id == 5U);
    assert(request.motor == 1U);
    assert(request.throttle_millionths == 20000U);
    request = parse("{\"type\":\"command\",\"request_id\":6,"
                    "\"command\":\"motor_test\",\"motor\":255,"
                    "\"throttle\":0.000001}");
    assert(request.motor == UINT8_MAX);
    assert(request.throttle_millionths == 1U);
    assert(parse("{\"type\":\"command\",\"request_id\":7,"
                 "\"command\":\"config_read\"}").command ==
           USB_JSON_COMMAND_CONFIG_READ);
    assert(parse("{\"type\":\"command\",\"request_id\":9,"
                 "\"command\":\"config_reset\"}").command ==
           USB_JSON_COMMAND_CONFIG_RESET);
    request = parse(
        "{\"type\":\"command\",\"request_id\":8,"
        "\"command\":\"config_write\",\"configuration\":{"
        "\"schema_version\":7,\"motors\":{\"propeller_layout\":"
        "\"PROPS_OUT\",\"directions\":[\"REVERSED\",\"NORMAL\","
        "\"NORMAL\",\"REVERSED\"]},"
        "\"control\":{"
        "\"roll\":{\"deadband\":0.03,\"maximum_angle_degrees\":30.0,"
        "\"maximum_rate_dps\":180.0,\"curve\":{\"type\":\"CONTROL_POINTS\","
        "\"interpolation\":\"LINEAR\",\"points\":[[0,0],[1,1]]}},"
        "\"pitch\":{\"deadband\":0.03,\"maximum_angle_degrees\":30.0,"
        "\"maximum_rate_dps\":180.0,\"curve\":{\"type\":\"CONTROL_POINTS\","
        "\"interpolation\":\"LINEAR\",\"points\":[[0,0],[1,1]]}},"
        "\"yaw\":{\"deadband\":0.04,\"maximum_angle_degrees\":0.0,"
        "\"maximum_rate_dps\":150.0,\"curve\":{\"type\":\"CONTROL_POINTS\","
        "\"interpolation\":\"LINEAR\",\"points\":[[0,0],[1,1]]}},"
        "\"throttle\":{\"zero_deadband\":0.02,\"maximum\":1.0,"
        "\"curve\":{\"type\":\"CONTROL_POINTS\","
        "\"interpolation\":\"LINEAR\",\"points\":[[0,0],[1,1]]}},"
        ATTITUDE_CONTROLLER_JSON RATE_CONTROLLER_JSON "},"
        "\"receiver_failsafe\":{\"stale_after_us\":25000,"
        "\"loss_detected_after_us\":100000,\"hold_last_until_us\":400000,"
        "\"stage_two_after_us\":1500000,\"recovery_stable_us\":500000,"
        "\"stage_one_roll\":-0.1,\"stage_one_pitch\":0.0,"
        "\"stage_one_yaw\":0.0,\"stage_one_throttle\":0.05,"
        "\"recovery_throttle_maximum\":0.05},\"imu\":{"
        "\"gyro_calibration\":{\"settling_duration_us\":100000,"
        "\"sample_duration_us\":500000,\"maximum_rate_dps\":5.0,"
        "\"maximum_standard_deviation_dps\":0.5},\"gyro_filter\":{"
        "\"type\":\"FIRST_ORDER_LOW_PASS\",\"cutoff_hz\":80.0},"
        "\"attitude_estimator\":{\"type\":\"COMPLEMENTARY\","
        "\"accelerometer_correction_time_constant_s\":0.5,"
        "\"maximum_gap_us\":10000}}}}");
    assert(request.command == USB_JSON_COMMAND_CONFIG_WRITE);
    assert(request.configuration.propeller_layout == 1U);
    assert(request.configuration.directions[0] == 1U);
    assert(request.configuration.failsafe_control_millionths[0] == -100000);
    assert(request.configuration.gyro_timing_us[1] == 500000U);
    assert(request.configuration.gyro_threshold_millionths[0] == 5000000U);
    assert(request.configuration.gyro_filter_cutoff_millionths == 80000000U);
    assert(request.configuration
               .accelerometer_correction_time_constant_millionths ==
           500000U);
    assert(request.configuration.attitude_maximum_gap_us == 10000U);
    assert(request.configuration.rate_controller_maximum_gap_us == 10000U);
    assert(request.configuration.attitude_gain_millionths[0] == 4000000U);
    assert(request.configuration.rate_pid_millionths[0][0] == 2000U);
    assert(request.configuration.control_axis_millionths[0][0] == 30000U);
    assert(request.configuration.control_axis_millionths[0][1] == 30000000U);
    assert(request.configuration.curve_point_count[3] == 2U);
    assert(request.configuration.curve_point_millionths[3][1][1] ==
           1000000U);
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
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":1,\"throttle\":-0.1}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":1,\"throttle\":.1}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":1,\"throttle\":0.}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":1,\"throttle\":0.0000001}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":1,\"throttle\":1e-2}",
        "{\"type\":\"command\",\"command\":\"motor_test\","
        "\"request_id\":1,\"motor\":256,\"throttle\":0.1}",
        "{\"type\":\"command\",\"command\":\"status\","
        "\"request_id\":1,\"motor\":1,\"throttle\":0.1}",
        "{\"type\":\"command\",\"command\":\"control_trace_start\","
        "\"request_id\":1}",
        "{\"type\":\"command\",\"command\":\"control_trace_start\","
        "\"request_id\":1,\"level\":\"OFF\"}",
        "{\"type\":\"command\",\"command\":\"control_trace_start\","
        "\"request_id\":1,\"level\":\"high_rate\"}",
        "{\"type\":\"command\",\"command\":\"config_write\","
        "\"request_id\":1}",
        "{\"type\":\"command\",\"command\":\"config_read\","
        "\"request_id\":1,\"motor\":1}",
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
    char output[4096];
    size_t length;
    static const char status[] =
        "{\"type\":\"response\",\"request_id\":42,"
        "\"command\":\"status\",\"ok\":true,"
        "\"state\":\"DISARMED\",\"control_source\":\"NONE\","
        "\"uptime_us\":42,"
        "\"firmware_version\":\"0.1.0\","
        "\"build_id\":\"abcdef0-dirty\"}\n";
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
    static const char motor_accepted[] =
        "{\"type\":\"response\",\"request_id\":10,"
        "\"command\":\"motor_test\",\"ok\":true,"
        "\"state\":\"ARMED\",\"motor\":1,"
        "\"throttle\":0.020000}\n";
    static const char motor_rejected[] =
        "{\"type\":\"response\",\"request_id\":11,"
        "\"command\":\"motor_test\",\"ok\":false,"
        "\"state\":\"DISARMED\",\"motor\":2,"
        "\"throttle\":0.100000,\"error\":\"motor_not_allowed\"}\n";
    const usb_json_configuration_t configuration = {
        .schema_version = 7U,
        .timing_us = {25000U, 100000U, 400000U, 1500000U, 500000U},
        .failsafe_control_millionths = {-100000, 0, 0, 50000, 50000},
        .directions = {0U, 0U, 1U, 0U},
        .propeller_layout = 0U,
        .gyro_timing_us = {100000U, 500000U},
        .gyro_threshold_millionths = {5000000U, 500000U},
        .gyro_filter_cutoff_millionths = 80000000U,
        .accelerometer_correction_time_constant_millionths = 500000U,
        .attitude_maximum_gap_us = 10000U,
        .control_axis_millionths = {
            {30000U, 30000000U, 180000000U},
            {30000U, 30000000U, 180000000U},
            {40000U, 0U, 150000000U},
        },
        .throttle_millionths = {20000U, 1000000U},
        .attitude_gain_millionths = {4000000U, 4000000U},
        .rate_controller_maximum_gap_us = 10000U,
        .rate_pid_millionths = {
            {2000U, 1000U, 10U, 150000U, 300000U},
            {2000U, 1000U, 10U, 150000U, 300000U},
            {1500U, 800U, 0U, 100000U, 200000U},
        },
        .curve_point_millionths = {
            [0] = {{0U, 0U}, {1000000U, 1000000U}},
            [1] = {{0U, 0U}, {1000000U, 1000000U}},
            [2] = {{0U, 0U}, {1000000U, 1000000U}},
            [3] = {{0U, 0U}, {1000000U, 1000000U}},
        },
        .curve_point_count = {2U, 2U, 2U, 2U},
    };

    assert(usb_json_build_status_response("DISARMED", "NONE", 42U, 42U,
                                          "0.1.0", "abcdef0-dirty", output,
                                          sizeof(output), &length));
    assert(length == sizeof(status) - 1U);
    assert(memcmp(output, status, length) == 0);
    assert(usb_json_build_status_response("DISARMED",
                                          "RECEIVER",
                                          UINT32_MAX,
                                          UINT64_MAX,
                                          "0.1.0",
                                          "abcdef0",
                                          output,
                                          sizeof(output),
                                          &length));
    assert(strstr(output,
                  "\"request_id\":4294967295") != NULL);
    assert(strstr(output,
                  "\"uptime_us\":18446744073709551615,") != NULL);
    assert(!usb_json_build_status_response("DISARMED", "NONE", 1U, 1U,
                                           NULL, "build", output,
                                           sizeof(output), &length));
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_ARM, 7U, true,
                                              false,
                                              "ARMED", NULL, output,
                                              sizeof(output), &length));
    assert(memcmp(output, accepted, length) == 0);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_DISARM, 8U, false,
                                              false,
                                              "BOOT", "transition_rejected",
                                              output, sizeof(output), &length));
    assert(memcmp(output, rejected, length) == 0);
    assert(usb_json_build_transition_response(USB_JSON_COMMAND_ARM, 12U, true,
                                              true, "DISARMED", NULL, output,
                                              sizeof(output), &length));
    assert(strstr(output, "\"pending\":true") != NULL);
    assert(usb_json_build_configuration_response(
        USB_JSON_COMMAND_CONFIG_WRITE,
        13U,
        true,
        "PERSISTENT",
        &configuration,
        "DISARMED",
        NULL,
        output,
        sizeof(output),
        &length));
    assert(strstr(output, "\"command\":\"config_write\"") != NULL);
    assert(strstr(output, "\"propeller_layout\":\"PROPS_IN\"") != NULL);
    assert(strstr(output,
                  "\"directions\":[\"NORMAL\",\"NORMAL\","
                  "\"REVERSED\",\"NORMAL\"]") != NULL);
    assert(strstr(output, "\"stage_one_roll\":-0.100000") != NULL);
    assert(strstr(output, "\"rate_controller\":{\"type\":\"PID\"") !=
           NULL);
    assert(strstr(output,
                  "\"attitude_controller\":{\"roll_gain_per_s\":"
                  "4.000000") != NULL);
    assert_valid_json_line(output, length);
    assert(usb_json_build_configuration_response(
        USB_JSON_COMMAND_CONFIG_WRITE,
        14U,
        false,
        "DEFAULT",
        &configuration,
        "ARMED",
        "state_rejected",
        output,
        sizeof(output),
        &length));
    assert(strstr(output, "\"error\":\"state_rejected\"") != NULL);
    assert_valid_json_line(output, length);
    assert(usb_json_build_error_response(false, 0U, "invalid_request", output,
                                         sizeof(output), &length));
    assert(memcmp(output, error, length) == 0);
    assert(usb_json_build_error_response(true, 9U, "unsupported_command",
                                         output, sizeof(output), &length));
    assert(memcmp(output, correlated_error, length) == 0);
    assert(usb_json_build_motor_test_response(10U, true, 1U, 20000U,
                                              "ARMED", NULL, output,
                                              sizeof(output), &length));
    assert(memcmp(output, motor_accepted, length) == 0);
    assert(usb_json_build_motor_test_response(11U, false, 2U, 100000U,
                                              "DISARMED",
                                              "motor_not_allowed", output,
                                              sizeof(output), &length));
    assert(memcmp(output, motor_rejected, length) == 0);
    assert(!usb_json_build_motor_test_response(1U, true, 1U, 1000001U,
                                               "ARMED", NULL, output,
                                               sizeof(output), &length));
    assert(!usb_json_build_error_response(false, 0U, "invalid_request",
                                          output, 4U,
                                          &length));
    assert(length == 0U);
}

static void maximum_curve_response_fits_transport_capacity(void)
{
    usb_json_configuration_t configuration = {
        .schema_version = 7U,
        .timing_us = {25000U, 100000U, 400000U, 1500000U, 500000U},
        .failsafe_control_millionths = {0, 0, 0, 50000, 50000},
        .gyro_timing_us = {100000U, 500000U},
        .gyro_threshold_millionths = {5000000U, 500000U},
        .gyro_filter_cutoff_millionths = 80000000U,
        .accelerometer_correction_time_constant_millionths = 500000U,
        .attitude_maximum_gap_us = 10000U,
        .control_axis_millionths = {
            {30000U, 30000000U, 180000000U},
            {30000U, 30000000U, 180000000U},
            {40000U, 0U, 150000000U},
        },
        .throttle_millionths = {20000U, 1000000U},
        .attitude_gain_millionths = {4000000U, 4000000U},
        .rate_controller_maximum_gap_us = 10000U,
        .rate_pid_millionths = {
            {2000U, 1000U, 10U, 150000U, 300000U},
            {2000U, 1000U, 10U, 150000U, 300000U},
            {1500U, 800U, 0U, 100000U, 200000U},
        },
    };
    char output[4096];
    size_t curve;
    size_t length;
    size_t point;

    for (curve = 0U; curve < USB_JSON_CONFIGURATION_CURVE_COUNT; curve++) {
        configuration.curve_point_count[curve] =
            USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS;
        for (point = 0U;
             point < USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS;
             point++) {
            const uint32_t value =
                point == (USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS - 1U)
                    ? 1000000U
                    : (uint32_t)(point * 142857U);

            configuration.curve_point_millionths[curve][point][0] = value;
            configuration.curve_point_millionths[curve][point][1] = value;
        }
    }
    assert(usb_json_build_configuration_response(
        USB_JSON_COMMAND_CONFIG_READ,
        99U,
        true,
        "PERSISTENT",
        &configuration,
        "DISARMED",
        NULL,
        output,
        sizeof(output),
        &length));
    assert(length < sizeof(output));
    assert_valid_json_line(output, length);
}

int main(void)
{
    valid_commands_and_key_order_are_accepted();
    malformed_or_noncanonical_requests_are_rejected();
    response_builders_are_exact_and_bounded();
    maximum_curve_response_fits_transport_capacity();
    return 0;
}
