#ifndef OPENFLIGHTCOMPUTER_TASK_H
#define OPENFLIGHTCOMPUTER_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TASK_NAME_MAX_LENGTH 31U
#define TASK_REGISTRY_CAPACITY 16U

typedef uint8_t task_priority_t;

enum {
    TASK_PRIORITY_HIGHEST = 0U,
    TASK_PRIORITY_HIGH = 64U,
    TASK_PRIORITY_NORMAL = 128U,
    TASK_PRIORITY_LOW = 192U,
    TASK_PRIORITY_BACKGROUND = 255U,
};

typedef void (*task_callback_t)(void *context);

typedef struct {
    const char *name;
    uint32_t period_us;
    task_priority_t priority;
    task_callback_t callback;
    void *context;
} task_definition_t;

typedef struct {
    task_definition_t definition;
    uint64_t next_release_us;
    uint64_t last_start_us;
    uint32_t last_execution_time_us;
    uint32_t maximum_execution_time_us;
    uint32_t execution_count;
    uint32_t overrun_count;
    uint32_t registration_order;
    bool enabled;
} task_t;

typedef struct {
    task_t tasks[TASK_REGISTRY_CAPACITY];
    size_t count;
} task_registry_t;

typedef enum {
    TASK_REGISTRATION_OK = 0,
    TASK_REGISTRATION_INVALID_ARGUMENT,
    TASK_REGISTRATION_INVALID_NAME,
    TASK_REGISTRATION_INVALID_PERIOD,
    TASK_REGISTRATION_INVALID_CALLBACK,
    TASK_REGISTRATION_DUPLICATE_NAME,
    TASK_REGISTRATION_CAPACITY_EXCEEDED,
} task_registration_result_t;

void task_registry_initialize(task_registry_t *registry);
task_registration_result_t task_registry_register(
    task_registry_t *registry,
    const task_definition_t *definition);
size_t task_registry_count(const task_registry_t *registry);
const task_t *task_registry_task_at(const task_registry_t *registry,
                                    size_t index);

#endif
