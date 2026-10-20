#ifndef OPENFLIGHTCOMPUTER_APPLICATION_TASKS_H
#define OPENFLIGHTCOMPUTER_APPLICATION_TASKS_H

#include "receiver_inspection.h"
#include "task.h"

#include <stdbool.h>

const receiver_inspection_provider_t *
application_receiver_inspection_provider(void);

typedef struct {
    const task_definition_t *usb_service;
    const task_definition_t *status_indicator;
    const task_definition_t *blackbox_storage;
    bool receiver_service_available;
    bool imu_service_available;
} application_task_options_t;

task_registration_result_t application_tasks_register(
    const application_task_options_t *options);

#endif
