#include "motor_output.h"

#include <stddef.h>

static bool backend_is_valid(const motor_output_backend_t *backend)
{
    return (backend != NULL) && (backend->initialize != NULL) &&
           (backend->submit != NULL) && (backend->force_stop != NULL);
}

motor_output_init_result_t motor_output_initialize(
    motor_output_t *output,
    const motor_output_backend_t *backend)
{
    motor_output_backend_t backend_copy;
    motor_output_backend_init_result_t init_result;
    motor_output_backend_stop_result_t stop_result;

    if (output == NULL) {
        return MOTOR_OUTPUT_INIT_INVALID_ARGUMENT;
    }

    if (backend != NULL) {
        backend_copy = *backend;
    } else {
        backend_copy = (motor_output_backend_t){0};
    }
    *output = (motor_output_t){0};
    if (!backend_is_valid(&backend_copy)) {
        return MOTOR_OUTPUT_INIT_INVALID_ARGUMENT;
    }

    init_result = backend_copy.initialize(backend_copy.context);
    if (init_result != MOTOR_OUTPUT_BACKEND_INIT_OK) {
        return MOTOR_OUTPUT_INIT_BACKEND_ERROR;
    }

    stop_result = backend_copy.force_stop(backend_copy.context);
    if (stop_result != MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED) {
        return MOTOR_OUTPUT_INIT_INITIAL_STOP_ERROR;
    }

    output->backend = backend_copy;
    output->initialized = true;
    return MOTOR_OUTPUT_INIT_OK;
}

motor_output_submit_result_t motor_output_submit(
    motor_output_t *output,
    const motor_command_t *command)
{
    motor_command_t canonical_command;
    motor_output_backend_submit_result_t backend_result;

    if ((output == NULL) || (command == NULL)) {
        return MOTOR_OUTPUT_SUBMIT_INVALID_ARGUMENT;
    }
    if (!output->initialized || !backend_is_valid(&output->backend)) {
        return MOTOR_OUTPUT_SUBMIT_NOT_INITIALIZED;
    }
    if (!command->valid ||
        (motor_command_create(&canonical_command,
                              command->throttle,
                              command->timestamp_us) !=
         MOTOR_COMMAND_CREATE_OK)) {
        return MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND;
    }

    backend_result = output->backend.submit(&canonical_command,
                                            output->backend.context);
    switch (backend_result) {
    case MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED:
        return MOTOR_OUTPUT_SUBMIT_ACCEPTED;
    case MOTOR_OUTPUT_BACKEND_SUBMIT_BUSY:
        return MOTOR_OUTPUT_SUBMIT_BUSY;
    case MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR:
        return MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR;
    }

    return MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR;
}

motor_output_stop_result_t motor_output_force_stop(motor_output_t *output)
{
    motor_output_backend_stop_result_t backend_result;

    if (output == NULL) {
        return MOTOR_OUTPUT_STOP_INVALID_ARGUMENT;
    }
    if (!output->initialized || !backend_is_valid(&output->backend)) {
        return MOTOR_OUTPUT_STOP_NOT_INITIALIZED;
    }

    backend_result = output->backend.force_stop(output->backend.context);
    if (backend_result == MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED) {
        return MOTOR_OUTPUT_STOP_ACCEPTED;
    }

    return MOTOR_OUTPUT_STOP_BACKEND_ERROR;
}
