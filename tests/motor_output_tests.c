#include "motor_output.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    motor_output_backend_init_result_t init_result;
    motor_output_backend_submit_result_t submit_result;
    motor_output_backend_stop_result_t stop_result;
    motor_output_backend_status_t status_result;
    const motor_command_t *expected_caller_command;
    motor_command_t copied_command;
    uint32_t initialize_count;
    uint32_t submit_count;
    uint32_t stop_count;
    uint32_t status_count;
    bool submit_received_distinct_storage;
    bool copied_command_valid;
} fake_backend_t;

static motor_output_backend_init_result_t fake_initialize(void *context)
{
    fake_backend_t *fake = context;

    fake->initialize_count++;
    return fake->init_result;
}

static motor_output_backend_submit_result_t fake_submit(
    const motor_command_t *command,
    void *context)
{
    fake_backend_t *fake = context;

    fake->submit_count++;
    fake->submit_received_distinct_storage =
        command != fake->expected_caller_command;
    if (fake->submit_result == MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED) {
        fake->copied_command = *command;
        fake->copied_command_valid = true;
    }
    return fake->submit_result;
}

static motor_output_backend_stop_result_t fake_force_stop(void *context)
{
    fake_backend_t *fake = context;

    fake->stop_count++;
    return fake->stop_result;
}

static motor_output_backend_status_t fake_status(void *context)
{
    fake_backend_t *fake = context;

    fake->status_count++;
    return fake->status_result;
}

static fake_backend_t successful_fake(void)
{
    return (fake_backend_t){
        .init_result = MOTOR_OUTPUT_BACKEND_INIT_OK,
        .submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED,
        .stop_result = MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED,
        .status_result = MOTOR_OUTPUT_BACKEND_STATUS_IDLE,
    };
}

static motor_output_backend_t backend_for(fake_backend_t *fake)
{
    return (motor_output_backend_t){
        .initialize = fake_initialize,
        .submit = fake_submit,
        .force_stop = fake_force_stop,
        .status = fake_status,
        .context = fake,
    };
}

static motor_command_t command_for(float motor_1,
                                   float motor_2,
                                   float motor_3,
                                   float motor_4,
                                   uint64_t timestamp_us)
{
    const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
        motor_1,
        motor_2,
        motor_3,
        motor_4,
    };
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, timestamp_us) ==
           MOTOR_COMMAND_CREATE_OK);
    return command;
}

static void initialization_requires_complete_backend(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output = {.initialized = true};

    assert(motor_output_initialize(NULL, &backend) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    assert(motor_output_initialize(&output, NULL) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    assert(!output.initialized);

    backend = backend_for(&fake);
    backend.initialize = NULL;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    backend = backend_for(&fake);
    backend.submit = NULL;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    backend = backend_for(&fake);
    backend.force_stop = NULL;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    backend = backend_for(&fake);
    backend.status = NULL;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INVALID_ARGUMENT);
    assert(fake.initialize_count == 0U);
    assert(fake.stop_count == 0U);
}

static void initialization_requires_backend_and_initial_stop_success(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;

    fake.init_result = MOTOR_OUTPUT_BACKEND_INIT_ERROR;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_BACKEND_ERROR);
    assert(fake.initialize_count == 1U);
    assert(fake.stop_count == 0U);
    assert(!output.initialized);

    fake = successful_fake();
    backend = backend_for(&fake);
    fake.stop_result = MOTOR_OUTPUT_BACKEND_STOP_ERROR;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INITIAL_STOP_ERROR);
    assert(fake.initialize_count == 1U);
    assert(fake.stop_count == 1U);
    assert(!output.initialized);

    fake = successful_fake();
    backend = backend_for(&fake);
    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    assert(fake.initialize_count == 1U);
    assert(fake.stop_count == 1U);
    assert(output.initialized);
}

static void unknown_backend_initialization_results_fail_closed(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;

    fake.init_result = (motor_output_backend_init_result_t)99;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_BACKEND_ERROR);
    assert(!output.initialized);

    fake = successful_fake();
    backend = backend_for(&fake);
    fake.stop_result = (motor_output_backend_stop_result_t)99;
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_INITIAL_STOP_ERROR);
    assert(!output.initialized);
}

static void accepted_submission_is_copied_and_canonicalized(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;
    motor_command_t command = command_for(0.1f, 0.2f, 0.3f, 1.0f, 77U);

    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    fake.expected_caller_command = &command;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_ACCEPTED);
    assert(fake.submit_count == 1U);
    assert(fake.submit_received_distinct_storage);
    assert(fake.copied_command_valid);
    assert(fake.copied_command.throttle[0] == 0.1f);
    assert(fake.copied_command.throttle[1] == 0.2f);
    assert(fake.copied_command.throttle[2] == 0.3f);
    assert(fake.copied_command.throttle[3] == 1.0f);
    assert(fake.copied_command.timestamp_us == 77U);
    assert(fake.copied_command.valid);

    command.throttle[0] = 0.9f;
    command.timestamp_us = 88U;
    assert(fake.copied_command.throttle[0] == 0.1f);
    assert(fake.copied_command.timestamp_us == 77U);
}

static void submission_revalidates_manually_modified_commands(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;
    motor_command_t command = command_for(0.1f, 0.2f, 0.3f, 0.4f, 10U);

    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    command.throttle[0] = 0.0001f;
    fake.expected_caller_command = &command;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_ACCEPTED);
    assert(fake.copied_command.throttle[0] == 0.0f);

    command.throttle[1] = NAN;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND);
    command.throttle[1] = INFINITY;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND);
    command.throttle[1] = -0.1f;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND);
    command.throttle[1] = 1.1f;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND);
    command.throttle[1] = 0.2f;
    command.valid = false;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_COMMAND);
    assert(fake.submit_count == 1U);
}

static void submission_maps_backend_results(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;
    motor_command_t command = command_for(0.1f, 0.2f, 0.3f, 0.4f, 10U);

    assert(motor_output_submit(NULL, &command) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_ARGUMENT);
    assert(motor_output_submit(&output, NULL) ==
           MOTOR_OUTPUT_SUBMIT_INVALID_ARGUMENT);
    output = (motor_output_t){0};
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_NOT_INITIALIZED);

    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    fake.submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_BUSY;
    assert(motor_output_submit(&output, &command) == MOTOR_OUTPUT_SUBMIT_BUSY);
    fake.submit_result = MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR);
    fake.submit_result = (motor_output_backend_submit_result_t)99;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR);
}

static void backend_descriptor_is_copied_during_initialization(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output;
    motor_command_t command = command_for(0.1f, 0.2f, 0.3f, 0.4f, 10U);

    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    backend.submit = NULL;
    backend.force_stop = NULL;
    backend.status = NULL;
    backend.context = NULL;

    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_ACCEPTED);
    assert(motor_output_force_stop(&output) == MOTOR_OUTPUT_STOP_ACCEPTED);
    assert(fake.submit_count == 1U);
    assert(fake.stop_count == 2U);
}

static void status_maps_backend_results(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output = {0};

    assert(motor_output_status(NULL) ==
           MOTOR_OUTPUT_STATUS_INVALID_ARGUMENT);
    assert(motor_output_status(&output) ==
           MOTOR_OUTPUT_STATUS_NOT_INITIALIZED);
    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);

    assert(motor_output_status(&output) == MOTOR_OUTPUT_STATUS_IDLE);
    fake.status_result = MOTOR_OUTPUT_BACKEND_STATUS_BUSY;
    assert(motor_output_status(&output) == MOTOR_OUTPUT_STATUS_BUSY);
    fake.status_result = MOTOR_OUTPUT_BACKEND_STATUS_ERROR;
    assert(motor_output_status(&output) ==
           MOTOR_OUTPUT_STATUS_BACKEND_ERROR);
    fake.status_result = (motor_output_backend_status_t)99;
    assert(motor_output_status(&output) ==
           MOTOR_OUTPUT_STATUS_BACKEND_ERROR);
    assert(fake.status_count == 4U);
}

static void force_stop_has_no_busy_outcome(void)
{
    fake_backend_t fake = successful_fake();
    motor_output_backend_t backend = backend_for(&fake);
    motor_output_t output = {0};

    assert(motor_output_force_stop(NULL) == MOTOR_OUTPUT_STOP_INVALID_ARGUMENT);
    assert(motor_output_force_stop(&output) ==
           MOTOR_OUTPUT_STOP_NOT_INITIALIZED);

    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    assert(motor_output_force_stop(&output) == MOTOR_OUTPUT_STOP_ACCEPTED);
    assert(fake.stop_count == 2U);

    fake.stop_result = MOTOR_OUTPUT_BACKEND_STOP_ERROR;
    assert(motor_output_force_stop(&output) ==
           MOTOR_OUTPUT_STOP_BACKEND_ERROR);
    fake.stop_result = (motor_output_backend_stop_result_t)99;
    assert(motor_output_force_stop(&output) ==
           MOTOR_OUTPUT_STOP_BACKEND_ERROR);
    assert(fake.stop_count == 4U);
}

int main(void)
{
    initialization_requires_complete_backend();
    initialization_requires_backend_and_initial_stop_success();
    unknown_backend_initialization_results_fail_closed();
    accepted_submission_is_copied_and_canonicalized();
    submission_revalidates_manually_modified_commands();
    submission_maps_backend_results();
    backend_descriptor_is_copied_during_initialization();
    force_stop_has_no_busy_outcome();
    status_maps_backend_results();
    return 0;
}
