#include "motor_command.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

_Static_assert(sizeof(float) == 4U,
               "Motor commands require 32-bit single-precision float");
_Static_assert(FLT_RADIX == 2,
               "Motor commands require a binary floating-point radix");
_Static_assert(FLT_MANT_DIG == 24,
               "Motor commands require IEEE-754 single precision");
_Static_assert((FLT_MIN_EXP == -125) && (FLT_MAX_EXP == 128),
               "Motor commands require IEEE-754 single exponent range");

static bool throttle_is_valid(float throttle)
{
    return isfinite(throttle) && (throttle >= 0.0f) && (throttle <= 1.0f);
}

void motor_command_initialize(motor_command_t *command)
{
    if (command == NULL) {
        return;
    }

    *command = (motor_command_t){0};
}

motor_command_create_result_t motor_command_create(
    motor_command_t *command,
    const float throttle[MOTOR_COMMAND_MOTOR_COUNT],
    uint64_t timestamp_us)
{
    motor_command_t candidate = {0};
    size_t index;

    if ((command == NULL) || (throttle == NULL)) {
        return MOTOR_COMMAND_CREATE_INVALID_ARGUMENT;
    }

    for (index = 0U; index < MOTOR_COMMAND_MOTOR_COUNT; index++) {
        if (!throttle_is_valid(throttle[index])) {
            return MOTOR_COMMAND_CREATE_INVALID_THROTTLE;
        }

        candidate.throttle[index] =
            throttle[index] <= MOTOR_COMMAND_STOP_THRESHOLD
                ? 0.0f
                : throttle[index];
    }

    candidate.timestamp_us = timestamp_us;
    candidate.valid = true;
    *command = candidate;
    return MOTOR_COMMAND_CREATE_OK;
}

void motor_command_invalidate(motor_command_t *command)
{
    motor_command_initialize(command);
}

bool motor_command_is_fresh(const motor_command_t *command,
                            uint64_t now_us,
                            uint64_t timeout_us)
{
    if ((command == NULL) || !command->valid || (timeout_us == 0U) ||
        (now_us < command->timestamp_us)) {
        return false;
    }

    return (now_us - command->timestamp_us) <= timeout_us;
}
