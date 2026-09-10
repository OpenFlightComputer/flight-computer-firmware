#include "application_task_definitions.h"

#include "application_state.h"

#define DIAGNOSTIC_FAST_TASK_PERIOD_US UINT32_C(10000)

static void run_diagnostic_fast_task(void *context)
{
    (void)context;
    firmware_fast_task_executions++;
}

const task_definition_t *diagnostic_fast_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "diagnostic-fast",
        .period_us = DIAGNOSTIC_FAST_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_HIGH,
        .callback = run_diagnostic_fast_task,
    };

    return &definition;
}
