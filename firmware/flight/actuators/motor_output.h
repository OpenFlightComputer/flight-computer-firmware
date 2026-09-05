#ifndef OPENFLIGHTCOMPUTER_MOTOR_OUTPUT_H
#define OPENFLIGHTCOMPUTER_MOTOR_OUTPUT_H

#include "motor_command.h"

#include <stdbool.h>

typedef enum {
    MOTOR_OUTPUT_BACKEND_INIT_OK = 0,
    MOTOR_OUTPUT_BACKEND_INIT_ERROR,
} motor_output_backend_init_result_t;

typedef enum {
    MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED = 0,
    MOTOR_OUTPUT_BACKEND_SUBMIT_BUSY,
    MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR,
} motor_output_backend_submit_result_t;

typedef enum {
    MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED = 0,
    MOTOR_OUTPUT_BACKEND_STOP_ERROR,
} motor_output_backend_stop_result_t;

typedef enum {
    MOTOR_OUTPUT_BACKEND_STATUS_IDLE = 0,
    MOTOR_OUTPUT_BACKEND_STATUS_BUSY,
    MOTOR_OUTPUT_BACKEND_STATUS_ERROR,
} motor_output_backend_status_t;

typedef motor_output_backend_init_result_t
    (*motor_output_backend_initialize_t)(void *context);
typedef motor_output_backend_submit_result_t
    (*motor_output_backend_submit_t)(const motor_command_t *command,
                                     void *context);
typedef motor_output_backend_stop_result_t
    (*motor_output_backend_force_stop_t)(void *context);
typedef motor_output_backend_status_t
    (*motor_output_backend_status_fn_t)(void *context);

typedef struct {
    motor_output_backend_initialize_t initialize;
    motor_output_backend_submit_t submit;
    motor_output_backend_force_stop_t force_stop;
    motor_output_backend_status_fn_t status;
    void *context;
} motor_output_backend_t;

typedef struct {
    motor_output_backend_t backend;
    bool initialized;
} motor_output_t;

typedef enum {
    MOTOR_OUTPUT_INIT_OK = 0,
    MOTOR_OUTPUT_INIT_INVALID_ARGUMENT,
    MOTOR_OUTPUT_INIT_BACKEND_ERROR,
    MOTOR_OUTPUT_INIT_INITIAL_STOP_ERROR,
} motor_output_init_result_t;

typedef enum {
    MOTOR_OUTPUT_SUBMIT_ACCEPTED = 0,
    MOTOR_OUTPUT_SUBMIT_INVALID_ARGUMENT,
    MOTOR_OUTPUT_SUBMIT_NOT_INITIALIZED,
    MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND,
    MOTOR_OUTPUT_SUBMIT_BUSY,
    MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR,
} motor_output_submit_result_t;

typedef enum {
    MOTOR_OUTPUT_STOP_ACCEPTED = 0,
    MOTOR_OUTPUT_STOP_INVALID_ARGUMENT,
    MOTOR_OUTPUT_STOP_NOT_INITIALIZED,
    MOTOR_OUTPUT_STOP_BACKEND_ERROR,
} motor_output_stop_result_t;

typedef enum {
    MOTOR_OUTPUT_STATUS_IDLE = 0,
    MOTOR_OUTPUT_STATUS_BUSY,
    MOTOR_OUTPUT_STATUS_INVALID_ARGUMENT,
    MOTOR_OUTPUT_STATUS_NOT_INITIALIZED,
    MOTOR_OUTPUT_STATUS_BACKEND_ERROR,
} motor_output_status_t;

motor_output_init_result_t motor_output_initialize(
    motor_output_t *output,
    const motor_output_backend_t *backend);
motor_output_submit_result_t motor_output_submit(
    motor_output_t *output,
    const motor_command_t *command);
motor_output_stop_result_t motor_output_force_stop(motor_output_t *output);
motor_output_status_t motor_output_status(const motor_output_t *output);

#endif
