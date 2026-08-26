#include "task.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t callback_invocation_count;

static void test_callback(void *context)
{
    uint32_t *value = context;

    callback_invocation_count++;
    if (value != NULL) {
        (*value)++;
    }
}

static task_definition_t definition_for(const char *name,
                                        uint32_t period_us,
                                        task_priority_t priority)
{
    return (task_definition_t){
        .name = name,
        .period_us = period_us,
        .priority = priority,
        .callback = test_callback,
        .context = &callback_invocation_count,
    };
}

static void initializes_empty_registry(void)
{
    task_registry_t registry;

    task_registry_initialize(NULL);
    task_registry_initialize(&registry);

    assert(task_registry_count(&registry) == 0U);
    assert(task_registry_task_at(&registry, 0U) == NULL);
    assert(task_registry_count(NULL) == 0U);
    assert(task_registry_task_at(NULL, 0U) == NULL);
}

static void accepts_maximum_name_length(void)
{
    static const char maximum_name[] =
        "1234567890123456789012345678901";
    task_registry_t registry;
    const task_definition_t definition =
        definition_for(maximum_name, 1000U, TASK_PRIORITY_NORMAL);

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_OK);
    assert(task_registry_count(&registry) == 1U);
}

static void registers_definition_and_initializes_runtime_metadata(void)
{
    task_registry_t registry;
    const task_definition_t definition =
        definition_for("flight-control", 1000U, TASK_PRIORITY_HIGHEST);
    const task_t *task;

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_OK);

    task = task_registry_task_at(&registry, 0U);
    assert(task != NULL);
    assert(task->definition.name == definition.name);
    assert(task->definition.period_us == 1000U);
    assert(task->definition.priority == TASK_PRIORITY_HIGHEST);
    assert(task->definition.callback == test_callback);
    assert(task->definition.context == &callback_invocation_count);
    assert(task->next_release_us == 0U);
    assert(task->last_start_us == 0U);
    assert(task->last_execution_time_us == 0U);
    assert(task->maximum_execution_time_us == 0U);
    assert(task->execution_count == 0U);
    assert(task->overrun_count == 0U);
    assert(task->missed_release_count == 0U);
    assert(task->registration_order == 0U);
    assert(task->enabled);
    assert(callback_invocation_count == 0U);
}

static void rejects_invalid_definitions(void)
{
    task_registry_t registry;
    task_definition_t definition =
        definition_for("valid", 1000U, TASK_PRIORITY_NORMAL);
    static const char overlong_name[] =
        "12345678901234567890123456789012";

    task_registry_initialize(&registry);

    assert(task_registry_register(NULL, &definition) ==
           TASK_REGISTRATION_INVALID_ARGUMENT);
    assert(task_registry_register(&registry, NULL) ==
           TASK_REGISTRATION_INVALID_ARGUMENT);

    definition.name = NULL;
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_INVALID_NAME);
    definition.name = "";
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_INVALID_NAME);
    definition.name = overlong_name;
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_INVALID_NAME);

    definition = definition_for("zero-period", 0U, TASK_PRIORITY_NORMAL);
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_INVALID_PERIOD);

    definition = definition_for("no-callback", 1000U, TASK_PRIORITY_NORMAL);
    definition.callback = NULL;
    assert(task_registry_register(&registry, &definition) ==
           TASK_REGISTRATION_INVALID_CALLBACK);

    assert(task_registry_count(&registry) == 0U);
}

static void rejects_duplicate_names(void)
{
    task_registry_t registry;
    const task_definition_t first =
        definition_for("receiver", 4000U, TASK_PRIORITY_HIGH);
    const task_definition_t duplicate =
        definition_for("receiver", 5000U, TASK_PRIORITY_LOW);

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &first) ==
           TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &duplicate) ==
           TASK_REGISTRATION_DUPLICATE_NAME);
    assert(task_registry_count(&registry) == 1U);
}

static void preserves_registration_order_and_priority_values(void)
{
    task_registry_t registry;
    const task_definition_t first =
        definition_for("background", 100000U, TASK_PRIORITY_BACKGROUND);
    const task_definition_t second =
        definition_for("normal", 10000U, TASK_PRIORITY_NORMAL);

    task_registry_initialize(&registry);
    assert(task_registry_register(&registry, &first) ==
           TASK_REGISTRATION_OK);
    assert(task_registry_register(&registry, &second) ==
           TASK_REGISTRATION_OK);

    assert(task_registry_task_at(&registry, 0U)->registration_order == 0U);
    assert(task_registry_task_at(&registry, 0U)->definition.priority ==
           TASK_PRIORITY_BACKGROUND);
    assert(task_registry_task_at(&registry, 1U)->registration_order == 1U);
    assert(task_registry_task_at(&registry, 1U)->definition.priority ==
           TASK_PRIORITY_NORMAL);
    assert(task_registry_task_at(&registry, 2U) == NULL);
}

static void enforces_fixed_capacity(void)
{
    static const char *const names[TASK_REGISTRY_CAPACITY + 1U] = {
        "task-00", "task-01", "task-02", "task-03", "task-04", "task-05",
        "task-06", "task-07", "task-08", "task-09", "task-10", "task-11",
        "task-12", "task-13", "task-14", "task-15", "task-16",
    };
    task_registry_t registry;
    size_t index;

    task_registry_initialize(&registry);
    for (index = 0U; index < TASK_REGISTRY_CAPACITY; index++) {
        const task_definition_t definition =
            definition_for(names[index], 1000U, (task_priority_t)index);

        assert(task_registry_register(&registry, &definition) ==
               TASK_REGISTRATION_OK);
    }

    {
        const task_definition_t extra =
            definition_for(names[TASK_REGISTRY_CAPACITY],
                           1000U,
                           TASK_PRIORITY_LOW);

        assert(task_registry_register(&registry, &extra) ==
               TASK_REGISTRATION_CAPACITY_EXCEEDED);
    }
    assert(task_registry_count(&registry) == TASK_REGISTRY_CAPACITY);
}

int main(void)
{
    initializes_empty_registry();
    accepts_maximum_name_length();
    registers_definition_and_initializes_runtime_metadata();
    rejects_invalid_definitions();
    rejects_duplicate_names();
    preserves_registration_order_and_priority_values();
    enforces_fixed_capacity();
    return 0;
}
