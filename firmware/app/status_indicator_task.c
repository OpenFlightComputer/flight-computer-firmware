#include "application_task_definitions.h"

#include "status_indicator.h"

#define STATUS_INDICATOR_TASK_PERIOD_US UINT32_C(100000)

static task_callback_result_t run_status_indicator_task(void *context)
{
    (void)context;
    status_indicator_process_pending();
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *status_indicator_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "status-indicator",
        .period_us = STATUS_INDICATOR_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_BACKGROUND,
        .callback = run_status_indicator_task,
        .context = NULL,
    };

    return &definition;
}
