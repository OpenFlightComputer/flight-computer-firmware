#ifndef OPENFLIGHTCOMPUTER_APPLICATION_TASK_DEFINITIONS_H
#define OPENFLIGHTCOMPUTER_APPLICATION_TASK_DEFINITIONS_H

#include "task.h"
#include "usb_command_processor.h"

const task_definition_t *motor_control_task_definition(void);
const task_definition_t *imu_task_definition(void);
const task_definition_t *receiver_task_definition(void);
const task_definition_t *flight_control_task_definition(void);
const task_definition_t *diagnostic_fast_task_definition(void);
const task_definition_t *diagnostic_medium_task_definition(void);
const task_definition_t *diagnostic_slow_task_definition(void);
const task_definition_t *usb_service_task_definition(void);

const receiver_inspection_provider_t *receiver_task_inspection_provider(void);

#endif
