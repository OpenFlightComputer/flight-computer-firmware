#include "task.h"

static bool task_name_is_valid(const char *name)
{
    size_t index;

    if (name == NULL) {
        return false;
    }

    for (index = 0U; index <= TASK_NAME_MAX_LENGTH; index++) {
        if (name[index] == '\0') {
            return index > 0U;
        }
    }

    return false;
}

static bool task_names_equal(const char *left, const char *right)
{
    size_t index;

    for (index = 0U; index <= TASK_NAME_MAX_LENGTH; index++) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }

    return false;
}

void task_registry_initialize(task_registry_t *registry)
{
    if (registry != NULL) {
        *registry = (task_registry_t){0};
    }
}

task_registration_result_t task_registry_register(
    task_registry_t *registry,
    const task_definition_t *definition)
{
    task_t task;
    size_t index;

    if ((registry == NULL) || (definition == NULL)) {
        return TASK_REGISTRATION_INVALID_ARGUMENT;
    }
    if (!task_name_is_valid(definition->name)) {
        return TASK_REGISTRATION_INVALID_NAME;
    }
    if (definition->period_us == 0U) {
        return TASK_REGISTRATION_INVALID_PERIOD;
    }
    if (definition->callback == NULL) {
        return TASK_REGISTRATION_INVALID_CALLBACK;
    }

    for (index = 0U; index < registry->count; index++) {
        if (task_names_equal(registry->tasks[index].definition.name,
                             definition->name)) {
            return TASK_REGISTRATION_DUPLICATE_NAME;
        }
    }

    if (registry->count >= TASK_REGISTRY_CAPACITY) {
        return TASK_REGISTRATION_CAPACITY_EXCEEDED;
    }

    task = (task_t){
        .definition = *definition,
        .registration_order = (uint32_t)registry->count,
        .enabled = true,
    };
    registry->tasks[registry->count] = task;
    registry->count++;

    return TASK_REGISTRATION_OK;
}

size_t task_registry_count(const task_registry_t *registry)
{
    if (registry == NULL) {
        return 0U;
    }

    return registry->count;
}

const task_t *task_registry_task_at(const task_registry_t *registry,
                                    size_t index)
{
    if ((registry == NULL) || (index >= registry->count)) {
        return NULL;
    }

    return &registry->tasks[index];
}
