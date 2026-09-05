#ifndef OPENFLIGHTCOMPUTER_USB_JSON_PROTOCOL_H
#define OPENFLIGHTCOMPUTER_USB_JSON_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    USB_JSON_COMMAND_STATUS = 0,
    USB_JSON_COMMAND_HEALTH,
    USB_JSON_COMMAND_ARM,
    USB_JSON_COMMAND_DISARM,
    USB_JSON_COMMAND_MOTOR_TEST,
    USB_JSON_COMMAND_UNSUPPORTED,
    USB_JSON_COMMAND_INVALID,
} usb_json_command_t;

typedef struct {
    usb_json_command_t command;
    uint32_t request_id;
    uint32_t throttle_millionths;
    uint8_t motor;
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
                                    uint32_t request_id,
                                    uint64_t uptime_us,
                                    const char *firmware_version,
                                    const char *build_id,
                                    char *destination,
                                    size_t capacity,
                                    size_t *length);
#endif
