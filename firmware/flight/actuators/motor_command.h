#ifndef OPENFLIGHTCOMPUTER_MOTOR_COMMAND_H
#define OPENFLIGHTCOMPUTER_MOTOR_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_COMMAND_MOTOR_COUNT 4U
#define MOTOR_COMMAND_STOP_THRESHOLD 0.001f
#define MOTOR_COMMAND_DEFAULT_TIMEOUT_US UINT64_C(100000)

typedef struct {
    float throttle[MOTOR_COMMAND_MOTOR_COUNT];
    uint64_t timestamp_us;
    bool valid;
} motor_command_t;

typedef enum {
    MOTOR_COMMAND_CREATE_OK = 0,
    MOTOR_COMMAND_CREATE_INVALID_ARGUMENT,
    MOTOR_COMMAND_CREATE_INVALID_THROTTLE,
} motor_command_create_result_t;

void motor_command_initialize(motor_command_t *command);
motor_command_create_result_t motor_command_create(
    motor_command_t *command,
    const float throttle[MOTOR_COMMAND_MOTOR_COUNT],
    uint64_t timestamp_us);
void motor_command_invalidate(motor_command_t *command);
bool motor_command_is_fresh(const motor_command_t *command,
                            uint64_t now_us,
                            uint64_t timeout_us);

#endif
