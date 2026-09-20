#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_CORE_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_CORE_H

#include "pitch_attitude_controller.h"
#include "quad_x_mixer.h"
#include "rate_controller.h"
#include "roll_attitude_controller.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CONTROL_OBJECTIVE_AXIS_DISABLED = 0,
    CONTROL_OBJECTIVE_AXIS_ANGLE,
    CONTROL_OBJECTIVE_AXIS_RATE,
} control_objective_axis_mode_t;

typedef struct {
    float attitude_degrees[RATE_CONTROLLER_AXIS_COUNT];
    float angular_rate_dps[RATE_CONTROLLER_AXIS_COUNT];
    uint64_t acquired_at_us;
    uint64_t source_sequence;
    bool valid;
} vehicle_state_t;

typedef struct {
    control_objective_axis_mode_t axis_mode[RATE_CONTROLLER_AXIS_COUNT];
    float axis_value[RATE_CONTROLLER_AXIS_COUNT];
    float throttle;
    uint64_t produced_at_us;
    uint64_t valid_until_us;
    bool valid;
} control_objective_t;

typedef struct {
    roll_attitude_controller_config_t roll_attitude;
    pitch_attitude_controller_config_t pitch_attitude;
    float maximum_rate_dps[RATE_CONTROLLER_AXIS_COUNT];
    prepared_quad_x_mixer_t mixer;
    float armed_idle;
    bool initialized;
} prepared_control_profile_t;

typedef struct {
    rate_controller_t *rate_controller;
    float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT];
    rate_controller_output_t rate_output;
    rate_controller_result_t rate_result;
    bool initialized;
} flight_control_core_t;

typedef struct {
    control_objective_t objective;
    float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT];
    rate_controller_output_t rate_output;
    rate_controller_result_t rate_result;
    quad_x_mixer_output_t mixer_output;
    float motor_baseline;
    bool mixer_output_valid;
} flight_control_output_t;

typedef enum {
    FLIGHT_CONTROL_CORE_UPDATED = 0,
    FLIGHT_CONTROL_CORE_WAITING_FOR_STATE,
    FLIGHT_CONTROL_CORE_INVALID_INPUT,
    FLIGHT_CONTROL_CORE_NOT_INITIALIZED,
} flight_control_core_result_t;

bool flight_control_profile_prepare(
    const roll_attitude_controller_config_t *roll_attitude,
    const pitch_attitude_controller_config_t *pitch_attitude,
    const float maximum_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    const prepared_quad_x_mixer_t *mixer,
    float armed_idle,
    prepared_control_profile_t *profile);
bool flight_control_core_initialize(flight_control_core_t *core,
                                    rate_controller_t *rate_controller);
void flight_control_core_reset(flight_control_core_t *core);
flight_control_core_result_t flight_control_update(
    flight_control_core_t *core,
    const prepared_control_profile_t *profile,
    const vehicle_state_t *state,
    const control_objective_t *objective,
    uint64_t now_us,
    flight_control_output_t *output);

#endif
