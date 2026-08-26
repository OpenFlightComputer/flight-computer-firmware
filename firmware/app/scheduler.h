#ifndef OPENFLIGHTCOMPUTER_SCHEDULER_H
#define OPENFLIGHTCOMPUTER_SCHEDULER_H

#include "task.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t (*scheduler_clock_t)(void);

typedef enum {
    SCHEDULER_INIT_OK = 0,
    SCHEDULER_INIT_INVALID_ARGUMENT,
} scheduler_init_result_t;

typedef enum {
    SCHEDULER_STEP_IDLE = 0,
    SCHEDULER_STEP_TASK_EXECUTED,
    SCHEDULER_STEP_INVALID_STATE,
} scheduler_step_result_t;

typedef struct {
    task_registry_t *registry;
    scheduler_clock_t clock;
    uint32_t ready_mask;
    uint32_t last_executed_task_index;
    bool initialized;
} scheduler_t;

scheduler_init_result_t scheduler_initialize(scheduler_t *scheduler,
                                             task_registry_t *registry,
                                             scheduler_clock_t clock);
scheduler_step_result_t scheduler_run_once(scheduler_t *scheduler);

#endif
