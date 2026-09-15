#include "application_task_definitions.h"

#include "application_state.h"

#define BLACKBOX_STORAGE_TASK_PERIOD_US UINT32_C(1000)

static task_callback_result_t run_blackbox_storage_task(void *context)
{
    blackbox_t *blackbox = context;
    if (blackbox->status == BLACKBOX_STATUS_NO_MEDIA) {
        return TASK_CALLBACK_DISABLE;
    }
    blackbox_service(blackbox);
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *blackbox_storage_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "blackbox-storage",
        .period_us = BLACKBOX_STORAGE_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_BACKGROUND,
        .callback = run_blackbox_storage_task,
        .context = &firmware_blackbox,
    };
    return &definition;
}
