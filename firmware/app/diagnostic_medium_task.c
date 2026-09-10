#include "application_task_definitions.h"

#include "application_state.h"
#include "logging.h"

#include <limits.h>
#include <stddef.h>

#define DIAGNOSTIC_MEDIUM_TASK_PERIOD_US UINT32_C(100000)
#define HIGH_RATE_PERIOD_US UINT32_C(1000)
#define HIGH_RATE_WARNING_PERMILLE UINT32_C(700)

static bool high_rate_budget_warning_logged;

static void run_diagnostic_medium_task(void *context)
{
    uint64_t budget_us = 0U;
    size_t index;

    (void)context;
    for (index = 0U; index < firmware_task_registry.count; index++) {
        const task_t *task = &firmware_task_registry.tasks[index];

        if (task->enabled &&
            (task->definition.period_us == HIGH_RATE_PERIOD_US)) {
            budget_us += task->maximum_execution_time_us;
        }
    }
    firmware_high_rate_worst_case_budget_us =
        budget_us > UINT32_MAX ? UINT32_MAX : (uint32_t)budget_us;
    firmware_high_rate_worst_case_utilization_permille =
        budget_us > (UINT32_MAX / UINT32_C(1000))
            ? UINT32_MAX
            : (uint32_t)((budget_us * UINT32_C(1000)) /
                         HIGH_RATE_PERIOD_US);
    if (!high_rate_budget_warning_logged &&
        (firmware_high_rate_worst_case_utilization_permille >=
         HIGH_RATE_WARNING_PERMILLE)) {
        LOG_WARN(LOG_MODULE_SCHEDULER,
                 "high-rate worst-case budget=%luus utilization=%lu/1000",
                 (unsigned long)firmware_high_rate_worst_case_budget_us,
                 (unsigned long)
                     firmware_high_rate_worst_case_utilization_permille);
        high_rate_budget_warning_logged = true;
    }
    firmware_medium_task_executions++;
}

const task_definition_t *diagnostic_medium_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "diagnostic-medium",
        .period_us = DIAGNOSTIC_MEDIUM_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_NORMAL,
        .callback = run_diagnostic_medium_task,
    };

    return &definition;
}
