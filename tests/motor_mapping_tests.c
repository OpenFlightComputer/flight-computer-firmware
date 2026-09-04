#include "motor_mapping.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static motor_command_t command_with_distinct_throttles(void)
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

static void assert_identity(const motor_mapping_t *mapping)
{
    size_t motor;

    assert(motor_mapping_is_valid(mapping));
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        assert(mapping->logical_to_physical[motor] == motor);
    }
}

static void initialization_selects_identity_mapping(void)
{
    motor_mapping_t mapping = {{3U, 2U, 1U, 0U}};

    motor_mapping_initialize(NULL);
    motor_mapping_initialize(&mapping);

    assert_identity(&mapping);
    assert(!motor_mapping_is_valid(NULL));
}

static void configuration_requires_disarmed_and_stopped(void)
{
    static const uint8_t permutation[MOTOR_COMMAND_MOTOR_COUNT] = {
        3U,
        0U,
        1U,
        2U,
    };
    motor_mapping_t mapping;

    motor_mapping_initialize(&mapping);

    assert(motor_mapping_configure(&mapping, permutation, false, false) ==
           MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE);
    assert_identity(&mapping);
    assert(motor_mapping_configure(&mapping, permutation, false, true) ==
           MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE);
    assert_identity(&mapping);
    assert(motor_mapping_configure(&mapping, permutation, true, false) ==
           MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE);
    assert_identity(&mapping);
    assert(motor_mapping_configure(&mapping, permutation, true, true) ==
           MOTOR_MAPPING_CONFIGURE_OK);
    assert(mapping.logical_to_physical[0] == 3U);
    assert(mapping.logical_to_physical[1] == 0U);
    assert(mapping.logical_to_physical[2] == 1U);
    assert(mapping.logical_to_physical[3] == 2U);
}

static void invalid_permutations_are_rejected_atomically(void)
{
    static const uint8_t duplicate[MOTOR_COMMAND_MOTOR_COUNT] = {
        0U,
        0U,
        2U,
        3U,
    };
    static const uint8_t out_of_range[MOTOR_COMMAND_MOTOR_COUNT] = {
        0U,
        1U,
        2U,
        4U,
    };
    motor_mapping_t mapping;

    motor_mapping_initialize(&mapping);

    assert(motor_mapping_configure(&mapping, duplicate, true, true) ==
           MOTOR_MAPPING_CONFIGURE_INVALID_PERMUTATION);
    assert_identity(&mapping);
    assert(motor_mapping_configure(&mapping, out_of_range, true, true) ==
           MOTOR_MAPPING_CONFIGURE_INVALID_PERMUTATION);
    assert_identity(&mapping);
    assert(motor_mapping_configure(NULL, duplicate, true, true) ==
           MOTOR_MAPPING_CONFIGURE_INVALID_ARGUMENT);
    assert(motor_mapping_configure(&mapping, NULL, true, true) ==
           MOTOR_MAPPING_CONFIGURE_INVALID_ARGUMENT);
    assert_identity(&mapping);
}

static bool assignment_is_expected_permutation(
    const uint8_t assignment[MOTOR_COMMAND_MOTOR_COUNT])
{
    bool seen[MOTOR_COMMAND_MOTOR_COUNT] = {false};
    size_t logical_motor;

    for (logical_motor = 0U; logical_motor < MOTOR_COMMAND_MOTOR_COUNT;
         logical_motor++) {
        if ((assignment[logical_motor] >= MOTOR_COMMAND_MOTOR_COUNT) ||
            seen[assignment[logical_motor]]) {
            return false;
        }
        seen[assignment[logical_motor]] = true;
    }

    return true;
}

static void all_in_range_assignments_are_classified(void)
{
    uint8_t assignment[MOTOR_COMMAND_MOTOR_COUNT];

    for (assignment[0] = 0U; assignment[0] < MOTOR_COMMAND_MOTOR_COUNT;
         assignment[0]++) {
        for (assignment[1] = 0U; assignment[1] < MOTOR_COMMAND_MOTOR_COUNT;
             assignment[1]++) {
            for (assignment[2] = 0U;
                 assignment[2] < MOTOR_COMMAND_MOTOR_COUNT;
                 assignment[2]++) {
                for (assignment[3] = 0U;
                     assignment[3] < MOTOR_COMMAND_MOTOR_COUNT;
                     assignment[3]++) {
                    motor_mapping_t mapping;
                    const bool expected_valid =
                        assignment_is_expected_permutation(assignment);

                    motor_mapping_initialize(&mapping);
                    const motor_mapping_configure_result_t result =
                        motor_mapping_configure(&mapping,
                                                assignment,
                                                true,
                                                true);

                    assert((result == MOTOR_MAPPING_CONFIGURE_OK) ==
                           expected_valid);
                    if (expected_valid) {
                        assert(mapping.logical_to_physical[0] == assignment[0]);
                        assert(mapping.logical_to_physical[1] == assignment[1]);
                        assert(mapping.logical_to_physical[2] == assignment[2]);
                        assert(mapping.logical_to_physical[3] == assignment[3]);
                    } else {
                        assert_identity(&mapping);
                    }
                }
            }
        }
    }
}

static void configured_mapping_reorders_complete_commands(void)
{
    static const uint8_t permutation[MOTOR_COMMAND_MOTOR_COUNT] = {
        3U,
        0U,
        1U,
        2U,
    };
    motor_mapping_t mapping;
    motor_command_t logical_command = command_with_distinct_throttles();
    motor_command_t physical_command;

    motor_mapping_initialize(&mapping);
    assert(motor_mapping_configure(&mapping, permutation, true, true) ==
           MOTOR_MAPPING_CONFIGURE_OK);
    motor_command_initialize(&physical_command);

    assert(motor_mapping_apply(&mapping,
                               &logical_command,
                               &physical_command) == MOTOR_MAPPING_APPLY_OK);
    assert(physical_command.throttle[0] == 0.2f);
    assert(physical_command.throttle[1] == 0.3f);
    assert(physical_command.throttle[2] == 0.4f);
    assert(physical_command.throttle[3] == 0.1f);
    assert(physical_command.timestamp_us == logical_command.timestamp_us);
    assert(physical_command.valid);
}

static void mapping_can_be_applied_in_place(void)
{
    static const uint8_t reverse[MOTOR_COMMAND_MOTOR_COUNT] = {
        3U,
        2U,
        1U,
        0U,
    };
    motor_mapping_t mapping;
    motor_command_t command = command_with_distinct_throttles();

    motor_mapping_initialize(&mapping);
    assert(motor_mapping_configure(&mapping, reverse, true, true) ==
           MOTOR_MAPPING_CONFIGURE_OK);
    assert(motor_mapping_apply(&mapping, &command, &command) ==
           MOTOR_MAPPING_APPLY_OK);
    assert(command.throttle[0] == 0.4f);
    assert(command.throttle[1] == 0.3f);
    assert(command.throttle[2] == 0.2f);
    assert(command.throttle[3] == 0.1f);
    assert(command.timestamp_us == 1234U);
    assert(command.valid);
}

static void invalid_apply_inputs_preserve_destination(void)
{
    motor_mapping_t mapping;
    motor_command_t logical_command = command_with_distinct_throttles();
    motor_command_t destination = command_with_distinct_throttles();
    const motor_command_t original = destination;

    motor_mapping_initialize(&mapping);

    assert(motor_mapping_apply(NULL, &logical_command, &destination) ==
           MOTOR_MAPPING_APPLY_INVALID_ARGUMENT);
    assert(motor_mapping_apply(&mapping, NULL, &destination) ==
           MOTOR_MAPPING_APPLY_INVALID_ARGUMENT);
    assert(motor_mapping_apply(&mapping, &logical_command, NULL) ==
           MOTOR_MAPPING_APPLY_INVALID_ARGUMENT);

    mapping.logical_to_physical[1] = mapping.logical_to_physical[0];
    assert(motor_mapping_apply(&mapping,
                               &logical_command,
                               &destination) ==
           MOTOR_MAPPING_APPLY_INVALID_MAPPING);

    motor_mapping_initialize(&mapping);
    logical_command.valid = false;
    assert(motor_mapping_apply(&mapping,
                               &logical_command,
                               &destination) ==
           MOTOR_MAPPING_APPLY_INVALID_COMMAND);

    assert(destination.throttle[0] == original.throttle[0]);
    assert(destination.throttle[1] == original.throttle[1]);
    assert(destination.throttle[2] == original.throttle[2]);
    assert(destination.throttle[3] == original.throttle[3]);
    assert(destination.timestamp_us == original.timestamp_us);
    assert(destination.valid == original.valid);
}

static void manually_corrupted_commands_are_rejected(void)
{
    motor_mapping_t mapping;
    motor_command_t logical_command = command_with_distinct_throttles();
    motor_command_t destination = command_with_distinct_throttles();
    const motor_command_t original = destination;

    motor_mapping_initialize(&mapping);
    logical_command.throttle[2] = 1.1f;

    assert(motor_mapping_apply(&mapping,
                               &logical_command,
                               &destination) ==
           MOTOR_MAPPING_APPLY_INVALID_COMMAND);
    assert(destination.timestamp_us == original.timestamp_us);
    assert(destination.throttle[2] == original.throttle[2]);
}

int main(void)
{
    initialization_selects_identity_mapping();
    configuration_requires_disarmed_and_stopped();
    invalid_permutations_are_rejected_atomically();
    all_in_range_assignments_are_classified();
    configured_mapping_reorders_complete_commands();
    mapping_can_be_applied_in_place();
    invalid_apply_inputs_preserve_destination();
    manually_corrupted_commands_are_rejected();
    return 0;
}
