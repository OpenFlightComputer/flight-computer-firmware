#include "application_task_definitions.h"

#include "application_state.h"
#include "gyro_calibration.h"
#include "logging.h"
#include "status_indicator.h"

#define STARTUP_TASK_PERIOD_US UINT32_C(10000)

static task_callback_result_t run_startup_task(void *context)
{
    system_state_transition_result_t result;

    (void)context;
    if ((firmware_system_state_machine.current !=
         SYSTEM_STATE_INITIALIZING) ||
        !firmware_gyro_calibration.initialized ||
        (firmware_gyro_calibration.state != GYRO_CALIBRATION_READY)) {
        return TASK_CALLBACK_CONTINUE;
    }

    (void)status_indicator_show_state(SYSTEM_STATE_DISARMED);
    result = system_state_machine_handle_event(
        &firmware_system_state_machine,
        SYSTEM_STATE_EVENT_INITIALIZATION_COMPLETED);
    firmware_system_state_last_result = (uint32_t)result;
    if (result == SYSTEM_STATE_TRANSITION_OK) {
        firmware_boot_status = BOOT_STATUS_RUNNING;
        LOG_INFO(LOG_MODULE_IMU,
                 "gyro calibration ready bias=%ld,%ld,%ld samples=%lu",
                 (long)firmware_gyro_calibration.bias[0],
                 (long)firmware_gyro_calibration.bias[1],
                 (long)firmware_gyro_calibration.bias[2],
                 (unsigned long)firmware_gyro_calibration.sample_count);
        LOG_INFO(LOG_MODULE_STATE, "INITIALIZING -> DISARMED");
        LOG_INFO(LOG_MODULE_SYSTEM, "firmware running");
        return TASK_CALLBACK_DISABLE;
    }
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *startup_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "startup",
        .period_us = STARTUP_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_NORMAL,
        .callback = run_startup_task,
        .context = NULL,
    };

    return &definition;
}
