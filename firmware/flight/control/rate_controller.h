#ifndef OPENFLIGHTCOMPUTER_RATE_CONTROLLER_H
#define OPENFLIGHTCOMPUTER_RATE_CONTROLLER_H

#include "rate_pid.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RATE_CONTROLLER_AXIS_ROLL = 0,
    RATE_CONTROLLER_AXIS_PITCH,
    RATE_CONTROLLER_AXIS_YAW,
    RATE_CONTROLLER_AXIS_COUNT,
} rate_controller_axis_t;

typedef enum {
    RATE_CONTROLLER_TYPE_PID = 0,
    RATE_CONTROLLER_TYPE_COUNT,
} rate_controller_type_t;

typedef struct {
    rate_controller_type_t type;
    uint32_t maximum_gap_us;
    rate_pid_config_t axis[RATE_CONTROLLER_AXIS_COUNT];
} rate_controller_config_t;

typedef enum {
    RATE_CONTROLLER_RESULT_UPDATED = 0,
    RATE_CONTROLLER_RESULT_SEEDED,
    RATE_CONTROLLER_RESULT_DISABLED,
    RATE_CONTROLLER_RESULT_NO_NEW_SAMPLE,
    RATE_CONTROLLER_RESULT_CONTINUITY_LOST,
    RATE_CONTROLLER_RESULT_INVALID_INPUT,
    RATE_CONTROLLER_RESULT_NOT_INITIALIZED,
} rate_controller_result_t;

typedef struct {
    rate_pid_output_t axis[RATE_CONTROLLER_AXIS_COUNT];
    uint64_t acquired_at_us;
    uint64_t source_sequence;
    uint32_t dt_us;
    bool valid;
} rate_controller_output_t;

typedef struct {
    rate_controller_config_t config;
    rate_pid_t axis[RATE_CONTROLLER_AXIS_COUNT];
    uint64_t previous_acquired_at_us;
    uint64_t previous_source_sequence;
    bool sample_seeded;
    bool initialized;
} rate_controller_t;

bool rate_controller_config_is_valid(const rate_controller_config_t *config);
bool rate_controller_initialize(rate_controller_t *controller,
                                const rate_controller_config_t *config);
void rate_controller_reset(rate_controller_t *controller);
rate_controller_result_t rate_controller_process(
    rate_controller_t *controller,
    const float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    const float measured_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    uint64_t acquired_at_us,
    uint64_t source_sequence,
    bool control_enabled,
    rate_controller_output_t *output);
const char *rate_controller_type_name(rate_controller_type_t type);

#endif
