#include "application_task_definitions.h"

#include "application_state.h"

#define DIAGNOSTIC_SLOW_TASK_PERIOD_US UINT32_C(1000000)

static task_callback_result_t run_diagnostic_slow_task(void *context)
{
    (void)context;
    firmware_slow_task_executions++;
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *diagnostic_slow_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "diagnostic-slow",
        .period_us = DIAGNOSTIC_SLOW_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_LOW,
        .callback = run_diagnostic_slow_task,
    };

    return &definition;
}
