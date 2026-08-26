#include "scheduler.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define EXECUTION_LOG_CAPACITY 32U

typedef struct {
    uint32_t identifier;
    uint64_t execution_time_us;
} callback_context_t;

static uint64_t fake_time_us;
static uint32_t execution_log[EXECUTION_LOG_CAPACITY];
static size_t execution_log_count;

static uint64_t fake_clock(void)
{
    return fake_time_us;
}

static void recording_callback(void *context)
{
    const callback_context_t *callback_context = context;

    assert(execution_log_count < EXECUTION_LOG_CAPACITY);
    execution_log[execution_log_count] = callback_context->identifier;
    execution_log_count++;
    fake_time_us += callback_context->execution_time_us;
}

static task_definition_t definition_for(const char *name,
                                        uint32_t period_us,
                                        task_priority_t priority,
                                        callback_context_t *context)
{
    return (task_definition_t){
        .name = name,
        .period_us = period_us,
        .priority = priority,
        .callback = recording_callback,
        .context = context,
    };
}

static void reset_fake_environment(uint64_t start_us)
{
    size_t index;

    fake_time_us = start_us;
    execution_log_count = 0U;
    for (index = 0U; index < EXECUTION_LOG_CAPACITY; index++) {
        execution_log[index] = 0U;
    }
}

static void rejects_invalid_state(void)
{
    task_registry_t registry;
    scheduler_t scheduler = {0};

    task_registry_initialize(&registry);
    reset_fake_environment(0U);

    assert(scheduler_initialize(NULL, &registry, fake_clock) ==
           SCHEDULER_INIT_INVALID_ARGUMENT);
    assert(scheduler_initialize(&scheduler, NULL, fake_clock) ==
           SCHEDULER_INIT_INVALID_ARGUMENT);
    assert(scheduler_initialize(&scheduler, &registry, NULL) ==
           SCHEDULER_INIT_INVALID_ARGUMENT);
    assert(scheduler_run_once(NULL) == SCHEDULER_STEP_INVALID_STATE);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_INVALID_STATE);
}

static void executes_immediately_and_records_timing(void)
{
    callback_context_t context = {
        .identifier = 1U,
        .execution_time_us = 25U,
    };
    const task_definition_t definition =
        definition_for("periodic", 1000U, TASK_PRIORITY_NORMAL, &context);
    task_registry_t registry;
    scheduler_t scheduler;
    const task_t *task;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_OK);
    reset_fake_environment(100U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);

    assert(registry.tasks[0].next_release_us == 100U);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);

    task = task_registry_task_at(&registry, 0U);
    assert(execution_log_count == 1U);
    assert(execution_log[0] == 1U);
    assert(task->last_start_us == 100U);
    assert(task->last_execution_time_us == 25U);
    assert(task->maximum_execution_time_us == 25U);
    assert(task->execution_count == 1U);
    assert(task->overrun_count == 0U);
    assert(task->missed_release_count == 0U);
    assert(task->next_release_us == 1100U);
    assert(scheduler.last_executed_task_index == 0U);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_IDLE);
}

static void selects_priority_then_release_then_registration_order(void)
{
    callback_context_t first_context = {.identifier = 1U};
    callback_context_t second_context = {.identifier = 2U};
    callback_context_t third_context = {.identifier = 3U};
    const task_definition_t first =
        definition_for("first", 100U, TASK_PRIORITY_NORMAL, &first_context);
    const task_definition_t second =
        definition_for("second", 100U, TASK_PRIORITY_HIGH, &second_context);
    const task_definition_t third =
        definition_for("third", 100U, TASK_PRIORITY_HIGH, &third_context);
    task_registry_t registry;
    scheduler_t scheduler;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &first) == TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &second) == TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &third) == TASK_REGISTRATION_OK);
    reset_fake_environment(20U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);

    registry.tasks[1].next_release_us = 10U;
    registry.tasks[2].next_release_us = 5U;

    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[0] == 3U);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[1] == 2U);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[2] == 1U);

    reset_fake_environment(20U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[0] == 2U);
}

static void ignores_disabled_tasks(void)
{
    callback_context_t disabled_context = {.identifier = 1U};
    callback_context_t enabled_context = {.identifier = 2U};
    const task_definition_t disabled_definition =
        definition_for("disabled",
                       100U,
                       TASK_PRIORITY_HIGHEST,
                       &disabled_context);
    const task_definition_t enabled_definition =
        definition_for("enabled", 100U, TASK_PRIORITY_LOW, &enabled_context);
    task_registry_t registry;
    scheduler_t scheduler;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &disabled_definition) ==
           TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &enabled_definition) ==
           TASK_REGISTRATION_OK);
    registry.tasks[0].enabled = false;
    reset_fake_environment(0U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);

    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log_count == 1U);
    assert(execution_log[0] == 2U);
    assert(registry.tasks[0].execution_count == 0U);
}

static void skips_missed_releases_and_detects_overrun(void)
{
    callback_context_t context = {
        .identifier = 1U,
        .execution_time_us = 250U,
    };
    const task_definition_t definition =
        definition_for("overrun", 100U, TASK_PRIORITY_HIGH, &context);
    task_registry_t registry;
    scheduler_t scheduler;
    const task_t *task;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_OK);
    reset_fake_environment(0U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);

    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    task = task_registry_task_at(&registry, 0U);
    assert(fake_time_us == 250U);
    assert(task->last_execution_time_us == 250U);
    assert(task->maximum_execution_time_us == 250U);
    assert(task->overrun_count == 1U);
    assert(task->missed_release_count == 2U);
    assert(task->next_release_us == 300U);
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_IDLE);

    fake_time_us = 299U;
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_IDLE);
    fake_time_us = 300U;
    context.execution_time_us = 10U;
    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(task->execution_count == 2U);
    assert(task->maximum_execution_time_us == 250U);
    assert(task->overrun_count == 1U);
    assert(task->next_release_us == 400U);
}

static void ready_batch_prevents_high_priority_starvation(void)
{
    callback_context_t low_context = {
        .identifier = 1U,
        .execution_time_us = 0U,
    };
    callback_context_t high_context = {
        .identifier = 2U,
        .execution_time_us = 2U,
    };
    const task_definition_t low_definition =
        definition_for("low", 100U, TASK_PRIORITY_LOW, &low_context);
    const task_definition_t high_definition =
        definition_for("high", 1U, TASK_PRIORITY_HIGHEST, &high_context);
    task_registry_t registry;
    scheduler_t scheduler;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &low_definition) ==
           TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &high_definition) ==
           TASK_REGISTRATION_OK);
    reset_fake_environment(0U);
    assert(scheduler_initialize(&scheduler, &registry, fake_clock) ==
           SCHEDULER_INIT_OK);

    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[0] == 2U);
    assert(fake_time_us == 2U);

    assert(scheduler_run_once(&scheduler) == SCHEDULER_STEP_TASK_EXECUTED);
    assert(execution_log[1] == 1U);
    assert(registry.tasks[0].execution_count == 1U);
    assert(registry.tasks[1].overrun_count == 1U);
    assert(registry.tasks[1].missed_release_count == 2U);
}

int main(void)
{
    rejects_invalid_state();
    executes_immediately_and_records_timing();
    selects_priority_then_release_then_registration_order();
    ignores_disabled_tasks();
    skips_missed_releases_and_detects_overrun();
    ready_batch_prevents_high_priority_starvation();
    return 0;
}
