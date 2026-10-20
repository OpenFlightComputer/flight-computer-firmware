#ifndef OPENFLIGHTCOMPUTER_APPLICATION_RUNTIME_H
#define OPENFLIGHTCOMPUTER_APPLICATION_RUNTIME_H

#include "flight_configuration_storage.h"
#include "imu_service.h"
#include "motor_output.h"
#include "receiver_source.h"
#include "scheduler.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const task_definition_t *usb_service;
    const task_definition_t *status_indicator;
    const task_definition_t *blackbox_storage;
} application_runtime_optional_tasks_t;

/*
 * Platform composition hook. It runs after the shared services are ready and
 * before task registration, so a platform can prepare only the optional
 * services it actually provides.
 */
typedef void (*application_runtime_prepare_optional_services_t)(
    void *context,
    application_runtime_optional_tasks_t *tasks);

typedef struct {
    scheduler_clock_t clock;
    /* A source with no read callback is unavailable and gets no service task. */
    imu_source_t imu_source;
    receiver_source_t receiver_source;
    flight_configuration_storage_t configuration_storage;
    motor_output_backend_t motor_output_backend;
    application_runtime_prepare_optional_services_t
        prepare_optional_services;
    void *optional_services_context;
} application_runtime_services_t;

typedef enum {
    APPLICATION_RUNTIME_INIT_OK = 0,
    APPLICATION_RUNTIME_INIT_INVALID_ARGUMENT,
    APPLICATION_RUNTIME_INIT_CORE_ERROR,
    APPLICATION_RUNTIME_INIT_CLOCK_ERROR,
    APPLICATION_RUNTIME_INIT_CONFIGURATION_ERROR,
    APPLICATION_RUNTIME_INIT_MOTOR_ERROR,
    APPLICATION_RUNTIME_INIT_IMU_ERROR,
    APPLICATION_RUNTIME_INIT_RECEIVER_ERROR,
    APPLICATION_RUNTIME_INIT_CALIBRATION_ERROR,
    APPLICATION_RUNTIME_INIT_TASK_ERROR,
    APPLICATION_RUNTIME_INIT_SCHEDULER_ERROR,
} application_runtime_init_result_t;

typedef enum {
    APPLICATION_RUNTIME_DRAIN_IDLE = 0,
    APPLICATION_RUNTIME_DRAIN_LIMIT_REACHED,
    APPLICATION_RUNTIME_DRAIN_INVALID_ARGUMENT,
    APPLICATION_RUNTIME_DRAIN_SCHEDULER_ERROR,
} application_runtime_drain_result_t;

void application_runtime_initialize(void);
application_runtime_init_result_t
application_runtime_initialize_with_services(
    const application_runtime_services_t *services);
application_runtime_drain_result_t application_runtime_run_until_idle(
    size_t maximum_task_executions,
    size_t *task_executions);
void application_runtime_run(void);

#endif
