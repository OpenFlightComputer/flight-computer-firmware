#ifndef OPENFLIGHTCOMPUTER_MOTOR_CONFIGURATION_H
#define OPENFLIGHTCOMPUTER_MOTOR_CONFIGURATION_H

#include "motor_command.h"

#include <stdbool.h>

typedef enum {
    MOTOR_DIRECTION_NORMAL = 0,
    MOTOR_DIRECTION_REVERSED,
    MOTOR_DIRECTION_COUNT,
} motor_direction_t;

typedef struct {
    motor_direction_t direction[MOTOR_COMMAND_MOTOR_COUNT];
} motor_configuration_t;

/* Compiled vehicle defaults used when no persistent override exists. */
void motor_configuration_defaults(motor_configuration_t *configuration);
bool motor_configuration_is_valid(
    const motor_configuration_t *configuration);
const char *motor_direction_name(motor_direction_t direction);

#endif
