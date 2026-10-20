#include "application_tasks.h"

#include "application_state.h"
#include "application_task_definitions.h"

#include <stddef.h>

static task_registration_result_t register_task(
    const task_definition_t *definition)
{
    return task_registry_register(&firmware_task_registry, definition);
}

const receiver_inspection_provider_t *
application_receiver_inspection_provider(void)
{
    return receiver_task_inspection_provider();
}

task_registration_result_t application_tasks_register(
    const application_task_options_t *options)
{
    const task_definition_t *diagnostic_tasks[] = {
        diagnostic_fast_task_definition(),
        diagnostic_medium_task_definition(),
        diagnostic_slow_task_definition(),
    };
    size_t index;
    task_registration_result_t result;

    task_registry_initialize(&firmware_task_registry);
    if (options == NULL) {
        return TASK_REGISTRATION_INVALID_ARGUMENT;
    }
    result = register_task(motor_control_task_definition());
    if (result != TASK_REGISTRATION_OK) {
        return result;
    }
    result = register_task(startup_task_definition());
    if (result != TASK_REGISTRATION_OK) {
        return result;
    }
    if (options->status_indicator != NULL) {
        result = register_task(options->status_indicator);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    if (options->blackbox_storage != NULL) {
        result = register_task(options->blackbox_storage);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    if (options->imu_service_available) {
        result = register_task(imu_task_definition());
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    if (options->receiver_service_available) {
        result = register_task(receiver_task_definition());
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
        result = register_task(flight_control_task_definition());
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    for (index = 0U;
         index < (sizeof(diagnostic_tasks) / sizeof(diagnostic_tasks[0]));
         index++) {
        result = register_task(diagnostic_tasks[index]);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    if (options->usb_service != NULL) {
        return register_task(options->usb_service);
    }
    return TASK_REGISTRATION_OK;
}
