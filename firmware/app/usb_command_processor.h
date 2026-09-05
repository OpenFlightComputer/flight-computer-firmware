#ifndef OPENFLIGHTCOMPUTER_USB_COMMAND_PROCESSOR_H
#define OPENFLIGHTCOMPUTER_USB_COMMAND_PROCESSOR_H

#include "fault.h"
#include "system_state.h"
#include "usb_cdc_transport.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t (*usb_command_clock_t)(void);

typedef enum {
    USB_COMMAND_PROCESS_IDLE = 0,
    USB_COMMAND_PROCESS_RESPONSE_SENT,
    USB_COMMAND_PROCESS_RESPONSE_PENDING,
    USB_COMMAND_PROCESS_TRANSPORT_ERROR,
    USB_COMMAND_PROCESS_BUILD_ERROR,
    USB_COMMAND_PROCESS_INVALID_STATE,
} usb_command_process_result_t;

typedef struct {
    uint32_t command_count;
    uint32_t malformed_count;
    uint32_t unsupported_count;
    uint32_t status_count;
    uint32_t health_count;
    uint32_t motor_test_count;
    uint32_t motor_test_accepted_count;
    uint32_t motor_test_rejected_count;
    uint32_t transition_accepted_count;
    uint32_t transition_rejected_count;
    uint32_t response_sent_count;
    uint32_t response_busy_count;
    uint32_t response_error_count;
    uint32_t response_build_error_count;
} usb_command_statistics_t;

typedef struct {
    system_state_machine_t *state_machine;
    fault_system_t *fault_system;
    usb_command_clock_t clock;
    const char *firmware_version;
    const char *build_id;
    char pending_response[USB_CDC_TRANSMIT_CAPACITY];
    size_t pending_response_length;
    usb_command_statistics_t statistics;
    system_state_transition_result_t last_transition_result;
    bool pending_response_valid;
    bool last_transition_valid;
    bool initialized;
} usb_command_processor_t;

typedef enum {
    USB_COMMAND_INIT_OK = 0,
    USB_COMMAND_INIT_INVALID_ARGUMENT,
} usb_command_init_result_t;

usb_command_init_result_t usb_command_processor_initialize(
    usb_command_processor_t *processor,
    system_state_machine_t *state_machine,
    fault_system_t *fault_system,
    usb_command_clock_t clock,
    const char *firmware_version,
    const char *build_id);
usb_command_process_result_t usb_command_processor_process_once(
    usb_command_processor_t *processor);

#endif
