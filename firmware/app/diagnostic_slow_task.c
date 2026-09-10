#include "application_task_definitions.h"

#include "application_state.h"

#define DIAGNOSTIC_SLOW_TASK_PERIOD_US UINT32_C(1000000)

static void run_diagnostic_slow_task(void *context)
{
    (void)context;
    firmware_slow_task_executions++;
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
