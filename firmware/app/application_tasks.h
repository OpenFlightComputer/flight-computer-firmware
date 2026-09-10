#ifndef OPENFLIGHTCOMPUTER_APPLICATION_TASKS_H
#define OPENFLIGHTCOMPUTER_APPLICATION_TASKS_H

#include "receiver_inspection.h"
#include "task.h"

#include <stdbool.h>

const receiver_inspection_provider_t *
application_receiver_inspection_provider(void);

task_registration_result_t application_tasks_register(
    bool usb_service_available,
    bool receiver_service_available,
    bool imu_service_available);

#endif
