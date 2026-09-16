#include "application_task_definitions.h"

#include "application_state.h"
#include "board.h"
#include "logging.h"
#include "usb_cdc_transport.h"

#define USB_SERVICE_TASK_PERIOD_US UINT32_C(2000)

static task_callback_result_t run_usb_service_task(void *context)
{
    (void)context;
    usb_cdc_transport_process();
    firmware_usb_connected = usb_cdc_transport_is_configured();
    firmware_usb_command_last_result =
        (uint32_t)usb_command_processor_process_once(
            &firmware_usb_command_processor);
    if (usb_command_processor_take_bootloader_handoff(
            &firmware_usb_command_processor)) {
        board_request_usb_bootloader();
    }
    if (firmware_usb_command_processor.last_transition_valid) {
        firmware_system_state_last_result =
            (uint32_t)firmware_usb_command_processor.last_transition_result;
    }
    if (!usb_command_processor_bootloader_handoff_pending(
            &firmware_usb_command_processor)) {
        firmware_logging_drain_last_result = (uint32_t)logging_drain_once();
    }
    firmware_usb_service_task_executions++;
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *usb_service_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "usb-service",
        .period_us = USB_SERVICE_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_BACKGROUND,
        .callback = run_usb_service_task,
    };

    return &definition;
}
