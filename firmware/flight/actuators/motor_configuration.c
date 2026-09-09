#include "motor_configuration.h"

#include <stddef.h>

bool motor_configuration_is_valid(
    const motor_configuration_t *configuration)
{
    size_t motor;

    if (configuration == NULL) {
        return false;
    }

    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        if ((configuration->direction[motor] != MOTOR_DIRECTION_NORMAL) &&
            (configuration->direction[motor] != MOTOR_DIRECTION_REVERSED)) {
            return false;
        }
    }

    return true;
}

const char *motor_direction_name(motor_direction_t direction)
{
    switch (direction) {
    case MOTOR_DIRECTION_NORMAL:
        return "NORMAL";
    case MOTOR_DIRECTION_REVERSED:
        return "REVERSED";
    case MOTOR_DIRECTION_COUNT:
        break;
    }

    return "INVALID";
}
