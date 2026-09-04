#ifndef OPENFLIGHTCOMPUTER_MOTOR_MAPPING_H
#define OPENFLIGHTCOMPUTER_MOTOR_MAPPING_H

#include "motor_command.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT];
} motor_mapping_t;

typedef enum {
    MOTOR_MAPPING_CONFIGURE_OK = 0,
    MOTOR_MAPPING_CONFIGURE_INVALID_ARGUMENT,
    MOTOR_MAPPING_CONFIGURE_UNSAFE_STATE,
    MOTOR_MAPPING_CONFIGURE_INVALID_PERMUTATION,
} motor_mapping_configure_result_t;

typedef enum {
    MOTOR_MAPPING_APPLY_OK = 0,
    MOTOR_MAPPING_APPLY_INVALID_ARGUMENT,
    MOTOR_MAPPING_APPLY_INVALID_MAPPING,
    MOTOR_MAPPING_APPLY_INVALID_COMMAND,
} motor_mapping_apply_result_t;

/* Initializes logical motor N to physical output N. */
void motor_mapping_initialize(motor_mapping_t *mapping);

/*
 * Replaces the complete mapping only when the system is disarmed and physical
 * outputs have already accepted a stop. The assignment must be a permutation
 * of physical output indices 0..3.
 */
motor_mapping_configure_result_t motor_mapping_configure(
    motor_mapping_t *mapping,
    const uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT],
    bool system_disarmed,
    bool outputs_stopped);

bool motor_mapping_is_valid(const motor_mapping_t *mapping);

/* Reorders one complete logical command into physical-output order. */
motor_mapping_apply_result_t motor_mapping_apply(
    const motor_mapping_t *mapping,
    const motor_command_t *logical_command,
    motor_command_t *physical_command);

#endif
