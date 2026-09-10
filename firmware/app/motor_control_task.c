#include "application_task_definitions.h"

#include "application_state.h"
#include "motor_control_internal.h"

static void run_motor_control_task(void *context)
{
    (void)context;
    firmware_motor_control_sync_last_result =
        (uint32_t)motor_control_synchronize();
    firmware_motor_control_task_executions++;
}

const task_definition_t *motor_control_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "motor-control",
        .period_us = MOTOR_CONTROL_FRAME_PERIOD_US,
        .priority = TASK_PRIORITY_HIGHEST,
        .callback = run_motor_control_task,
    };

    return &definition;
}
