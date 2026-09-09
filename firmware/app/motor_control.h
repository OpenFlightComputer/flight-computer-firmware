#ifndef OPENFLIGHTCOMPUTER_MOTOR_CONTROL_H
#define OPENFLIGHTCOMPUTER_MOTOR_CONTROL_H

#include "motor_command.h"
#include "motor_configuration.h"

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_CONTROL_FRAME_PERIOD_US UINT64_C(1000)
#define MOTOR_CONTROL_OUTPUT_COMPLETION_TIMEOUT_US \
    MOTOR_CONTROL_FRAME_PERIOD_US
#define MOTOR_CONTROL_ARMING_PREPARATION_US UINT64_C(5000000)
#define MOTOR_CONTROL_DIRECTION_COMMAND_REPETITIONS 10U

typedef enum {
    MOTOR_CONTROL_SOURCE_NONE = 0,
    MOTOR_CONTROL_SOURCE_USB_TEST,
    MOTOR_CONTROL_SOURCE_RECEIVER,
    MOTOR_CONTROL_SOURCE_COUNT,
} motor_control_source_t;

typedef enum {
    MOTOR_CONTROL_ARM_ACCEPTED = 0,
    MOTOR_CONTROL_ARM_PENDING,
    MOTOR_CONTROL_ARM_NOT_INITIALIZED,
    MOTOR_CONTROL_ARM_INVALID_SOURCE,
    MOTOR_CONTROL_ARM_BLOCKED_STATE,
    MOTOR_CONTROL_ARM_BLOCKED_HEALTH,
    MOTOR_CONTROL_ARM_BLOCKED_PREPARATION,
    MOTOR_CONTROL_ARM_TRANSITION_ERROR,
} motor_control_arm_result_t;

typedef enum {
    MOTOR_CONTROL_DISARM_ACCEPTED = 0,
    MOTOR_CONTROL_DISARM_NOT_INITIALIZED,
    MOTOR_CONTROL_DISARM_BLOCKED_STATE,
    MOTOR_CONTROL_DISARM_TRANSITION_ERROR,
} motor_control_disarm_result_t;

typedef enum {
    MOTOR_CONTROL_SUBMIT_ACCEPTED = 0,
    MOTOR_CONTROL_SUBMIT_NOT_INITIALIZED,
    MOTOR_CONTROL_SUBMIT_BLOCKED_STATE,
    MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE,
    MOTOR_CONTROL_SUBMIT_BLOCKED_PREPARATION,
    MOTOR_CONTROL_SUBMIT_BLOCKED_HEALTH,
    MOTOR_CONTROL_SUBMIT_INVALID_COMMAND,
    MOTOR_CONTROL_SUBMIT_STALE_COMMAND,
    MOTOR_CONTROL_SUBMIT_MAPPING_ERROR,
    MOTOR_CONTROL_SUBMIT_FORCE_STOP_ERROR,
} motor_control_submit_result_t;

typedef enum {
    MOTOR_CONTROL_SYNC_SAFE = 0,
    MOTOR_CONTROL_SYNC_STOPPED,
    MOTOR_CONTROL_SYNC_FAILSAFE_ENTERED,
    MOTOR_CONTROL_SYNC_BACKEND_ERROR,
    MOTOR_CONTROL_SYNC_NOT_INITIALIZED,
    MOTOR_CONTROL_SYNC_FORCE_STOP_ERROR,
} motor_control_sync_result_t;

typedef enum {
    MOTOR_CONTROL_STOP_ACCEPTED = 0,
    MOTOR_CONTROL_STOP_NOT_INITIALIZED,
    MOTOR_CONTROL_STOP_ERROR,
} motor_control_stop_result_t;

typedef enum {
    MOTOR_CONTROL_MAPPING_CONFIGURE_OK = 0,
    MOTOR_CONTROL_MAPPING_CONFIGURE_NOT_INITIALIZED,
    MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_ARGUMENT,
    MOTOR_CONTROL_MAPPING_CONFIGURE_UNSAFE_STATE,
    MOTOR_CONTROL_MAPPING_CONFIGURE_INVALID_PERMUTATION,
} motor_control_mapping_configure_result_t;

typedef enum {
    MOTOR_CONTROL_DIRECTION_CONFIGURE_OK = 0,
    MOTOR_CONTROL_DIRECTION_CONFIGURE_NOT_INITIALIZED,
    MOTOR_CONTROL_DIRECTION_CONFIGURE_INVALID_ARGUMENT,
    MOTOR_CONTROL_DIRECTION_CONFIGURE_UNSAFE_STATE,
    MOTOR_CONTROL_DIRECTION_CONFIGURE_STORAGE_ERROR,
} motor_control_direction_configure_result_t;

typedef enum {
    MOTOR_CONTROL_CONFIGURATION_RESET_OK = 0,
    MOTOR_CONTROL_CONFIGURATION_RESET_NOT_INITIALIZED,
    MOTOR_CONTROL_CONFIGURATION_RESET_UNSAFE_STATE,
    MOTOR_CONTROL_CONFIGURATION_RESET_STORAGE_ERROR,
} motor_control_configuration_reset_result_t;

motor_control_arm_result_t motor_control_arm(motor_control_source_t source);
motor_control_disarm_result_t motor_control_disarm(void);

motor_control_submit_result_t motor_control_submit(
    motor_control_source_t source,
    const motor_command_t *logical_command);

/* Call periodically even when no new command is available. */
motor_control_sync_result_t motor_control_synchronize(void);

motor_control_stop_result_t motor_control_force_stop(void);

motor_control_mapping_configure_result_t motor_control_configure_mapping(
    const uint8_t logical_to_physical[MOTOR_COMMAND_MOTOR_COUNT]);

motor_control_direction_configure_result_t motor_control_configure_direction(
    uint8_t logical_motor,
    motor_direction_t direction);
motor_control_configuration_reset_result_t
    motor_control_reset_configuration(void);
bool motor_control_get_configuration(motor_configuration_t *configuration,
                                     bool *persistent_override);

bool motor_control_is_initialized(void);
bool motor_control_outputs_stopped(void);
bool motor_control_ready_for_arm(void);
motor_control_source_t motor_control_active_source(void);
motor_control_source_t motor_control_pending_source(void);
const char *motor_control_source_name(motor_control_source_t source);

#endif
