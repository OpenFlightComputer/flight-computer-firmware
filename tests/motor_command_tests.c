#include "motor_command.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

static void assert_throttle(const motor_command_t *command,
                            float motor_1,
                            float motor_2,
                            float motor_3,
                            float motor_4)
{
    assert(command->throttle[0] == motor_1);
    assert(command->throttle[1] == motor_2);
    assert(command->throttle[2] == motor_3);
    assert(command->throttle[3] == motor_4);
}

static motor_command_t valid_command(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
        0.1f,
        0.2f,
        0.3f,
        0.4f,
    };
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, 1234U) ==
           MOTOR_COMMAND_CREATE_OK);
    return command;
}

static void initialization_is_invalid_and_stopped(void)
{
    motor_command_t command = {
        .throttle = {1.0f, 1.0f, 1.0f, 1.0f},
        .timestamp_us = UINT64_MAX,
        .valid = true,
    };

    motor_command_initialize(NULL);
    motor_command_initialize(&command);

    assert_throttle(&command, 0.0f, 0.0f, 0.0f, 0.0f);
    assert(command.timestamp_us == 0U);
    assert(!command.valid);
}

static void valid_values_are_accepted_atomically(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
        0.0f,
        0.25f,
        0.5f,
        1.0f,
    };
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, UINT64_MAX) ==
           MOTOR_COMMAND_CREATE_OK);
    assert_throttle(&command, 0.0f, 0.25f, 0.5f, 1.0f);
    assert(command.timestamp_us == UINT64_MAX);
    assert(command.valid);
}

static void near_zero_values_are_canonicalized_to_stop(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
        0.0f,
        0.0001f,
        MOTOR_COMMAND_STOP_THRESHOLD,
        0.0011f,
    };
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, 42U) ==
           MOTOR_COMMAND_CREATE_OK);
    assert_throttle(&command, 0.0f, 0.0f, 0.0f, 0.0011f);
}

static void all_zero_is_a_valid_stop_command(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {0};
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, 9U) ==
           MOTOR_COMMAND_CREATE_OK);
    assert_throttle(&command, 0.0f, 0.0f, 0.0f, 0.0f);
    assert(command.valid);
    assert(motor_command_is_fresh(&command, 9U, 1U));
}

static void invalid_values_are_rejected_without_replacing_command(void)
{
    static const float invalid_values[] = {
        -0.0001f,
        1.0001f,
        NAN,
        INFINITY,
        -INFINITY,
    };
    size_t invalid_index;

    for (invalid_index = 0U;
         invalid_index < sizeof(invalid_values) / sizeof(invalid_values[0]);
         invalid_index++) {
        motor_command_t command = valid_command();
        const motor_command_t original = command;
        float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
            0.1f,
            0.2f,
            0.3f,
            0.4f,
        };

        throttle[2] = invalid_values[invalid_index];
        assert(motor_command_create(&command, throttle, 9999U) ==
               MOTOR_COMMAND_CREATE_INVALID_THROTTLE);
        assert_throttle(&command,
                        original.throttle[0],
                        original.throttle[1],
                        original.throttle[2],
                        original.throttle[3]);
        assert(command.timestamp_us == original.timestamp_us);
        assert(command.valid == original.valid);
    }
}

static void invalid_arguments_leave_destination_unchanged(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {0};
    motor_command_t command = valid_command();
    const motor_command_t original = command;

    assert(motor_command_create(NULL, throttle, 1U) ==
           MOTOR_COMMAND_CREATE_INVALID_ARGUMENT);
    assert(motor_command_create(&command, NULL, 1U) ==
           MOTOR_COMMAND_CREATE_INVALID_ARGUMENT);
    assert_throttle(&command,
                    original.throttle[0],
                    original.throttle[1],
                    original.throttle[2],
                    original.throttle[3]);
    assert(command.timestamp_us == original.timestamp_us);
    assert(command.valid == original.valid);
}

static void invalidation_clears_all_command_data(void)
{
    motor_command_t command = valid_command();

    motor_command_invalidate(NULL);
    motor_command_invalidate(&command);

    assert_throttle(&command, 0.0f, 0.0f, 0.0f, 0.0f);
    assert(command.timestamp_us == 0U);
    assert(!command.valid);
}

static void freshness_has_explicit_boundaries(void)
{
    motor_command_t command = valid_command();

    assert(MOTOR_COMMAND_DEFAULT_TIMEOUT_US == UINT64_C(100000));
    assert(!motor_command_is_fresh(NULL, 1234U, 100U));
    assert(!motor_command_is_fresh(&command, 1234U, 0U));
    assert(!motor_command_is_fresh(&command, 1233U, 100U));
    assert(motor_command_is_fresh(&command, 1234U, 100U));
    assert(motor_command_is_fresh(&command, 1334U, 100U));
    assert(!motor_command_is_fresh(&command, 1335U, 100U));
    assert(motor_command_is_fresh(
        &command,
        1234U + MOTOR_COMMAND_DEFAULT_TIMEOUT_US,
        MOTOR_COMMAND_DEFAULT_TIMEOUT_US));
    assert(!motor_command_is_fresh(
        &command,
        1235U + MOTOR_COMMAND_DEFAULT_TIMEOUT_US,
        MOTOR_COMMAND_DEFAULT_TIMEOUT_US));

    motor_command_invalidate(&command);
    assert(!motor_command_is_fresh(&command, 1234U, 100U));
}

static void freshness_is_safe_near_uint64_limit(void)
{
    static const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {0};
    motor_command_t command;

    motor_command_initialize(&command);
    assert(motor_command_create(&command, throttle, UINT64_MAX - 10U) ==
           MOTOR_COMMAND_CREATE_OK);
    assert(motor_command_is_fresh(&command, UINT64_MAX, 10U));
    assert(!motor_command_is_fresh(&command, UINT64_MAX, 9U));
}

int main(void)
{
    initialization_is_invalid_and_stopped();
    valid_values_are_accepted_atomically();
    near_zero_values_are_canonicalized_to_stop();
    all_zero_is_a_valid_stop_command();
    invalid_values_are_rejected_without_replacing_command();
    invalid_arguments_leave_destination_unchanged();
    invalidation_clears_all_command_data();
    freshness_has_explicit_boundaries();
    freshness_is_safe_near_uint64_limit();
    return 0;
}
