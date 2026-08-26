#include "scheduler.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(TASK_REGISTRY_CAPACITY <= 32U,
               "scheduler ready mask is limited to 32 tasks");

static uint32_t task_bit(size_t index)
{
    return UINT32_C(1) << index;
}

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static void saturating_add(uint32_t *value, uint64_t amount)
{
    const uint64_t available = (uint64_t)UINT32_MAX - *value;

    if (amount >= available) {
        *value = UINT32_MAX;
    } else {
        *value += (uint32_t)amount;
    }
}

static uint32_t duration_to_u32(uint64_t duration_us)
{
    if (duration_us > UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)duration_us;
}

static void build_ready_batch(scheduler_t *scheduler, uint64_t now_us)
{
    size_t index;

    scheduler->ready_mask = 0U;

    for (index = 0U; index < scheduler->registry->count; index++) {
        const task_t *task = &scheduler->registry->tasks[index];

        if (task->enabled && (now_us >= task->next_release_us)) {
            scheduler->ready_mask |= task_bit(index);
        }
    }
}

static bool task_precedes(const task_t *candidate, const task_t *selected)
{
    if (candidate->definition.priority != selected->definition.priority) {
        return candidate->definition.priority < selected->definition.priority;
    }
    if (candidate->next_release_us != selected->next_release_us) {
        return candidate->next_release_us < selected->next_release_us;
    }

    return candidate->registration_order < selected->registration_order;
}

static size_t select_ready_task(scheduler_t *scheduler)
{
    size_t selected_index = TASK_REGISTRY_CAPACITY;
    size_t index;

    for (index = 0U; index < scheduler->registry->count; index++) {
        task_t *task;

        if ((scheduler->ready_mask & task_bit(index)) == 0U) {
            continue;
        }

        task = &scheduler->registry->tasks[index];
        if (!task->enabled) {
            scheduler->ready_mask &= ~task_bit(index);
            continue;
        }

        if ((selected_index == TASK_REGISTRY_CAPACITY) ||
            task_precedes(task,
                          &scheduler->registry->tasks[selected_index])) {
            selected_index = index;
        }
    }

    return selected_index;
}

static void record_task_execution(task_t *task,
                                  uint64_t start_us,
                                  uint64_t finish_us)
{
    const uint64_t duration_us = finish_us - start_us;
    const uint64_t elapsed_since_release_us =
        finish_us - task->next_release_us;
    const uint64_t periods_to_advance =
        (elapsed_since_release_us / task->definition.period_us) + 1U;
    const uint64_t missed_releases = periods_to_advance - 1U;

    task->last_start_us = start_us;
    task->last_execution_time_us = duration_to_u32(duration_us);
    if (task->last_execution_time_us > task->maximum_execution_time_us) {
        task->maximum_execution_time_us = task->last_execution_time_us;
    }
    saturating_increment(&task->execution_count);

    if (duration_us > task->definition.period_us) {
        saturating_increment(&task->overrun_count);
    }
    saturating_add(&task->missed_release_count, missed_releases);

    task->next_release_us +=
        periods_to_advance * task->definition.period_us;
}

scheduler_init_result_t scheduler_initialize(scheduler_t *scheduler,
                                             task_registry_t *registry,
                                             scheduler_clock_t clock)
{
    uint64_t start_us;
    size_t index;

    if ((scheduler == NULL) || (registry == NULL) || (clock == NULL)) {
        return SCHEDULER_INIT_INVALID_ARGUMENT;
    }

    start_us = clock();
    for (index = 0U; index < registry->count; index++) {
        task_t *task = &registry->tasks[index];

        task->next_release_us = start_us;
        task->last_start_us = 0U;
        task->last_execution_time_us = 0U;
        task->maximum_execution_time_us = 0U;
        task->execution_count = 0U;
        task->overrun_count = 0U;
        task->missed_release_count = 0U;
    }

    *scheduler = (scheduler_t){
        .registry = registry,
        .clock = clock,
        .last_executed_task_index = UINT32_MAX,
        .initialized = true,
    };

    return SCHEDULER_INIT_OK;
}

scheduler_step_result_t scheduler_run_once(scheduler_t *scheduler)
{
    size_t selected_index;
    task_t *task;
    uint64_t start_us;
    uint64_t finish_us;

    if ((scheduler == NULL) || !scheduler->initialized ||
        (scheduler->registry == NULL) || (scheduler->clock == NULL)) {
        return SCHEDULER_STEP_INVALID_STATE;
    }

    if (scheduler->ready_mask == 0U) {
        build_ready_batch(scheduler, scheduler->clock());
        if (scheduler->ready_mask == 0U) {
            return SCHEDULER_STEP_IDLE;
        }
    }

    selected_index = select_ready_task(scheduler);
    if (selected_index == TASK_REGISTRY_CAPACITY) {
        return SCHEDULER_STEP_IDLE;
    }

    scheduler->ready_mask &= ~task_bit(selected_index);
    scheduler->last_executed_task_index = (uint32_t)selected_index;
    task = &scheduler->registry->tasks[selected_index];

    start_us = scheduler->clock();
    task->definition.callback(task->definition.context);
    finish_us = scheduler->clock();
    record_task_execution(task, start_us, finish_us);

    return SCHEDULER_STEP_TASK_EXECUTED;
}
