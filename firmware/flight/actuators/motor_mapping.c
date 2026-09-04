#include "motor_mapping.h"

#include <stddef.h>

static bool assignment_is_permutation(
    const uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT])
{
    bool physical_output_seen[MOTOR_COMMAND_MOTOR_COUNT] = {false};
    size_t logical_motor;

    if (logical_to_physical == NULL) {
        return false;
    }

    for (logical_motor = 0U; logical_motor < MOTOR_COMMAND_MOTOR_COUNT;
         logical_motor++) {
        const uint8_t physical_output =
            logical_to_physical[logical_motor];

        if ((physical_output >= MOTOR_COMMAND_MOTOR_COUNT) ||
            physical_output_seen[physical_output]) {
            return false;
        }
        physical_output_seen[physical_output] = true;
    }

    return true;
}

void motor_mapping_initialize(motor_mapping_t *mapping)
{
    size_t motor;

    if (mapping == NULL) {
        return;
    }

    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        mapping->logical_to_physical[motor] = (uint8_t)motor;
    }
}

motor_mapping_configure_result_t motor_mapping_configure(
    motor_mapping_t *mapping,
    const uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT],
    bool system_disarmed,
    bool outputs_stopped)
{
    motor_mapping_t candidate;
    size_t logical_motor;

    if ((mapping == NULL) || (logical_to_physical == NULL)) {
        return MOTOR_MAPPING_CONFIGURE_INVALID_ARGUMENT;
    }
    if (!system_disarmed || !outputs_stopped) {
        return MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE;
    }
    if (!assignment_is_permutation(logical_to_physical)) {
        return MOTOR_MAPPING_CONFIGURE_INVALID_PERMUTATION;
    }

    for (logical_motor = 0U; logical_motor < MOTOR_COMMAND_MOTOR_COUNT;
         logical_motor++) {
        candidate.logical_to_physical[logical_motor] =
            logical_to_physical[logical_motor];
    }

    *mapping = candidate;
    return MOTOR_MAPPING_CONFIGURE_OK;
}

bool motor_mapping_is_valid(const motor_mapping_t *mapping)
{
    return (mapping != NULL) &&
           assignment_is_permutation(mapping->logical_to_physical);
}

motor_mapping_apply_result_t motor_mapping_apply(
    const motor_mapping_t *mapping,
    const motor_command_t *logical_command,
    motor_command_t *physical_command)
{
    float physical_throttle[MOTOR_COMMAND_MOTOR_COUNT] = {0.0f};
    uint64_t timestamp_us;
    size_t logical_motor;

    if ((mapping == NULL) || (logical_command == NULL) ||
        (physical_command == NULL)) {
        return MOTOR_MAPPING_APPLY_INVALID_ARGUMENT;
    }
    if (!motor_mapping_is_valid(mapping)) {
        return MOTOR_MAPPING_APPLY_INVALID_MAPPING;
    }
    if (!logical_command->valid) {
        return MOTOR_MAPPING_APPLY_INVALID_COMMAND;
    }

    timestamp_us = logical_command->timestamp_us;
    for (logical_motor = 0U; logical_motor < MOTOR_COMMAND_MOTOR_COUNT;
         logical_motor++) {
        const uint8_t physical_output =
            mapping->logical_to_physical[logical_motor];

        physical_throttle[physical_output] =
            logical_command->throttle[logical_motor];
    }

    if (motor_command_create(physical_command,
                             physical_throttle,
                             timestamp_us) != MOTOR_COMMAND_CREATE_OK) {
        return MOTOR_MAPPING_APPLY_INVALID_COMMAND;
    }

    return MOTOR_MAPPING_APPLY_OK;
}
