#include "motor_configuration.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
    motor_configuration_t configuration;
    size_t motor;

    motor_configuration_defaults(&configuration);
    assert(motor_configuration_is_valid(&configuration));
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        assert(configuration.direction[motor] == MOTOR_DIRECTION_NORMAL);
    }

    configuration.direction[2] = MOTOR_DIRECTION_REVERSED;
    assert(motor_configuration_is_valid(&configuration));
    configuration.direction[2] = MOTOR_DIRECTION_COUNT;
    assert(!motor_configuration_is_valid(&configuration));
    configuration.direction[2] = (motor_direction_t)-1;
    assert(!motor_configuration_is_valid(&configuration));
    assert(!motor_configuration_is_valid(NULL));
    assert(strcmp(motor_direction_name(MOTOR_DIRECTION_NORMAL), "NORMAL") == 0);
    assert(strcmp(motor_direction_name(MOTOR_DIRECTION_REVERSED),
                  "REVERSED") == 0);
    assert(strcmp(motor_direction_name(MOTOR_DIRECTION_COUNT), "INVALID") == 0);
    return 0;
}
