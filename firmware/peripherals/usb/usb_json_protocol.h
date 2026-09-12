#ifndef OPENFLIGHTCOMPUTER_USB_JSON_PROTOCOL_H
#define OPENFLIGHTCOMPUTER_USB_JSON_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    USB_JSON_COMMAND_STATUS = 0,
    USB_JSON_COMMAND_HEALTH,
    USB_JSON_COMMAND_RECEIVER,
    USB_JSON_COMMAND_IMU,
    USB_JSON_COMMAND_CONTROL_TRACE_START,
    USB_JSON_COMMAND_CONTROL_TRACE_READ,
    USB_JSON_COMMAND_CONTROL_TRACE_STOP,
    USB_JSON_COMMAND_ARM,
    USB_JSON_COMMAND_DISARM,
    USB_JSON_COMMAND_MOTOR_TEST,
    USB_JSON_COMMAND_CONFIG_READ,
    USB_JSON_COMMAND_CONFIG_WRITE,
    USB_JSON_COMMAND_CONFIG_RESET,
    USB_JSON_COMMAND_UNSUPPORTED,
    USB_JSON_COMMAND_INVALID,
} usb_json_command_t;

typedef enum {
    USB_JSON_TRACE_LEVEL_EVENTS = 0,
    USB_JSON_TRACE_LEVEL_LOW_RATE,
    USB_JSON_TRACE_LEVEL_HIGH_RATE,
    USB_JSON_TRACE_LEVEL_FULL_RATE,
    USB_JSON_TRACE_LEVEL_COUNT,
} usb_json_trace_level_t;

#define USB_JSON_CONFIGURATION_MOTOR_COUNT 4U
#define USB_JSON_CONFIGURATION_TIMING_COUNT 5U
#define USB_JSON_CONFIGURATION_CONTROL_COUNT 5U
#define USB_JSON_CONFIGURATION_GYRO_TIMING_COUNT 2U
#define USB_JSON_CONFIGURATION_GYRO_THRESHOLD_COUNT 2U
#define USB_JSON_CONFIGURATION_CURVE_COUNT 4U
#define USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS 8U

typedef struct {
    uint32_t schema_version;
    uint64_t timing_us[USB_JSON_CONFIGURATION_TIMING_COUNT];
    int32_t failsafe_control_millionths[
        USB_JSON_CONFIGURATION_CONTROL_COUNT];
    uint8_t directions[USB_JSON_CONFIGURATION_MOTOR_COUNT];
    uint8_t propeller_layout;
    uint64_t gyro_timing_us[USB_JSON_CONFIGURATION_GYRO_TIMING_COUNT];
    uint32_t gyro_threshold_millionths[
        USB_JSON_CONFIGURATION_GYRO_THRESHOLD_COUNT];
    uint32_t gyro_filter_cutoff_millionths;
    uint32_t accelerometer_correction_time_constant_millionths;
    uint32_t attitude_maximum_gap_us;
    uint8_t gyro_filter_type;
    uint8_t attitude_estimator_type;
    uint32_t control_axis_millionths[3][3];
    uint32_t throttle_millionths[2];
    uint32_t attitude_gain_millionths[2];
    uint32_t rate_controller_maximum_gap_us;
    uint32_t rate_pid_millionths[3][5];
    uint8_t rate_controller_type;
    uint32_t curve_point_millionths
        [USB_JSON_CONFIGURATION_CURVE_COUNT]
        [USB_JSON_CONFIGURATION_CURVE_MAXIMUM_POINTS][2];
    uint8_t curve_type[USB_JSON_CONFIGURATION_CURVE_COUNT];
    uint8_t curve_interpolation[USB_JSON_CONFIGURATION_CURVE_COUNT];
    uint8_t curve_point_count[USB_JSON_CONFIGURATION_CURVE_COUNT];
} usb_json_configuration_t;

typedef struct {
    usb_json_command_t command;
    uint32_t request_id;
    uint32_t throttle_millionths;
    uint8_t motor;
    usb_json_trace_level_t trace_level;
    usb_json_configuration_t configuration;
} usb_json_request_t;

bool usb_json_parse_request(const char *line,
                            size_t line_length,
                            usb_json_request_t *request);
const char *usb_json_command_name(usb_json_command_t command);

bool usb_json_build_error_response(bool request_id_valid,
                                   uint32_t request_id,
                                   const char *error,
                                   char *destination,
                                   size_t capacity,
                                   size_t *length);
bool usb_json_build_transition_response(usb_json_command_t command,
                                        uint32_t request_id,
                                        bool accepted,
                                        bool pending,
                                        const char *state,
                                        const char *error,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length);
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
    size_t *length);
bool usb_json_build_motor_test_response(uint32_t request_id,
                                        bool accepted,
                                        uint8_t motor,
                                        uint32_t throttle_millionths,
                                        const char *state,
                                        const char *error,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length);
bool usb_json_build_status_response(const char *state,
                                    const char *control_source,
                                    uint32_t request_id,
                                    uint64_t uptime_us,
                                    const char *firmware_version,
                                    const char *build_id,
                                    char *destination,
                                    size_t capacity,
                                    size_t *length);
#endif
