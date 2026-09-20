#include "flight_control_core.h"

#include "yaw_attitude_controller.h"

#include <math.h>
#include <stddef.h>

static bool unit_interval(float value)
{
    return isfinite(value) && (value >= 0.0F) && (value <= 1.0F);
}

static bool positive_finite(float value)
{
    return isfinite(value) && (value > 0.0F);
}

bool flight_control_profile_prepare(
    const roll_attitude_controller_config_t *roll_attitude,
    const pitch_attitude_controller_config_t *pitch_attitude,
    const float maximum_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    const prepared_quad_x_mixer_t *mixer,
    float armed_idle,
    prepared_control_profile_t *profile)
{
    size_t axis;

    if ((roll_attitude == NULL) || (pitch_attitude == NULL) ||
        (maximum_rate_dps == NULL) || (mixer == NULL) ||
        !mixer->initialized || !positive_finite(roll_attitude->gain_per_s) ||
        !positive_finite(pitch_attitude->gain_per_s) ||
        !unit_interval(armed_idle) || (profile == NULL)) {
        return false;
    }
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!positive_finite(maximum_rate_dps[axis])) {
            return false;
        }
    }
    *profile = (prepared_control_profile_t){
        .roll_attitude = *roll_attitude,
        .pitch_attitude = *pitch_attitude,
        .mixer = *mixer,
        .armed_idle = armed_idle,
        .initialized = true,
    };
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        profile->maximum_rate_dps[axis] = maximum_rate_dps[axis];
    }
    return true;
}

bool flight_control_core_initialize(flight_control_core_t *core,
                                    rate_controller_t *rate_controller)
{
    if ((core == NULL) || (rate_controller == NULL) ||
        !rate_controller->initialized) {
        return false;
    }
    *core = (flight_control_core_t){
        .rate_controller = rate_controller,
        .rate_result = RATE_CONTROLLER_RESULT_DISABLED,
        .initialized = true,
    };
    return true;
}

void flight_control_core_reset(flight_control_core_t *core)
{
    if ((core == NULL) || !core->initialized ||
        (core->rate_controller == NULL)) {
        return;
    }
    rate_controller_reset(core->rate_controller);
    core->desired_rate_dps[0] = 0.0F;
    core->desired_rate_dps[1] = 0.0F;
    core->desired_rate_dps[2] = 0.0F;
    core->rate_output = (rate_controller_output_t){0};
    core->rate_result = RATE_CONTROLLER_RESULT_DISABLED;
}

static bool prepare_desired_rates(
    flight_control_core_t *core,
    const prepared_control_profile_t *profile,
    const vehicle_state_t *state,
    const control_objective_t *objective)
{
    const control_objective_axis_mode_t roll_mode =
        objective->axis_mode[RATE_CONTROLLER_AXIS_ROLL];
    const control_objective_axis_mode_t pitch_mode =
        objective->axis_mode[RATE_CONTROLLER_AXIS_PITCH];
    const control_objective_axis_mode_t yaw_mode =
        objective->axis_mode[RATE_CONTROLLER_AXIS_YAW];

    if (roll_mode == CONTROL_OBJECTIVE_AXIS_ANGLE) {
        if (!roll_attitude_controller_update(
                &profile->roll_attitude,
                objective->axis_value[RATE_CONTROLLER_AXIS_ROLL],
                state->attitude_degrees[RATE_CONTROLLER_AXIS_ROLL],
                profile->maximum_rate_dps[RATE_CONTROLLER_AXIS_ROLL],
                &core->desired_rate_dps[RATE_CONTROLLER_AXIS_ROLL])) {
            return false;
        }
    } else if (roll_mode == CONTROL_OBJECTIVE_AXIS_RATE) {
        if (!yaw_attitude_controller_update(
                objective->axis_value[RATE_CONTROLLER_AXIS_ROLL],
                profile->maximum_rate_dps[RATE_CONTROLLER_AXIS_ROLL],
                &core->desired_rate_dps[RATE_CONTROLLER_AXIS_ROLL])) {
            return false;
        }
    } else if (roll_mode == CONTROL_OBJECTIVE_AXIS_DISABLED) {
        core->desired_rate_dps[RATE_CONTROLLER_AXIS_ROLL] = 0.0F;
    } else {
        return false;
    }
    if (pitch_mode == CONTROL_OBJECTIVE_AXIS_ANGLE) {
        if (!pitch_attitude_controller_update(
                &profile->pitch_attitude,
                objective->axis_value[RATE_CONTROLLER_AXIS_PITCH],
                state->attitude_degrees[RATE_CONTROLLER_AXIS_PITCH],
                profile->maximum_rate_dps[RATE_CONTROLLER_AXIS_PITCH],
                &core->desired_rate_dps[RATE_CONTROLLER_AXIS_PITCH])) {
            return false;
        }
    } else if (pitch_mode == CONTROL_OBJECTIVE_AXIS_RATE) {
        if (!yaw_attitude_controller_update(
                objective->axis_value[RATE_CONTROLLER_AXIS_PITCH],
                profile->maximum_rate_dps[RATE_CONTROLLER_AXIS_PITCH],
                &core->desired_rate_dps[RATE_CONTROLLER_AXIS_PITCH])) {
            return false;
        }
    } else if (pitch_mode == CONTROL_OBJECTIVE_AXIS_DISABLED) {
        core->desired_rate_dps[RATE_CONTROLLER_AXIS_PITCH] = 0.0F;
    } else {
        return false;
    }
    if (yaw_mode == CONTROL_OBJECTIVE_AXIS_RATE) {
        return yaw_attitude_controller_update(
            objective->axis_value[RATE_CONTROLLER_AXIS_YAW],
            profile->maximum_rate_dps[RATE_CONTROLLER_AXIS_YAW],
            &core->desired_rate_dps[RATE_CONTROLLER_AXIS_YAW]);
    }
    if (yaw_mode == CONTROL_OBJECTIVE_AXIS_DISABLED) {
        core->desired_rate_dps[RATE_CONTROLLER_AXIS_YAW] = 0.0F;
        return true;
    }
    return false;
}

static bool mix_idle(const prepared_control_profile_t *profile,
                     uint64_t now_us,
                     quad_x_mixer_output_t *mixer_output)
{
    static const float zero_correction[RATE_CONTROLLER_AXIS_COUNT];

    return quad_x_mixer_apply_prepared_with_floor(
        &profile->mixer,
        profile->armed_idle,
        profile->armed_idle,
        zero_correction,
        now_us,
        mixer_output);
}

flight_control_core_result_t flight_control_update(
    flight_control_core_t *core,
    const prepared_control_profile_t *profile,
    const vehicle_state_t *state,
    const control_objective_t *objective,
    uint64_t now_us,
    flight_control_output_t *output)
{
    float correction[RATE_CONTROLLER_AXIS_COUNT];
    float effective_throttle;
    size_t axis;

    if (output != NULL) {
        *output = (flight_control_output_t){0};
    }
    if ((core == NULL) || !core->initialized ||
        (core->rate_controller == NULL)) {
        return FLIGHT_CONTROL_CORE_NOT_INITIALIZED;
    }
    if ((profile == NULL) || !profile->initialized ||
        (objective == NULL) || !objective->valid ||
        (objective->produced_at_us > now_us) ||
        (objective->valid_until_us < now_us) ||
        (objective->valid_until_us < objective->produced_at_us) ||
        !unit_interval(objective->throttle) || (output == NULL)) {
        return FLIGHT_CONTROL_CORE_INVALID_INPUT;
    }
    output->objective = *objective;
    if (objective->throttle == 0.0F) {
        flight_control_core_reset(core);
        if (!mix_idle(profile, now_us, &output->mixer_output)) {
            return FLIGHT_CONTROL_CORE_INVALID_INPUT;
        }
        output->rate_result = core->rate_result;
        output->motor_baseline = profile->armed_idle;
        output->mixer_output_valid = true;
        return FLIGHT_CONTROL_CORE_UPDATED;
    }
    if ((state == NULL) || !state->valid ||
        !prepare_desired_rates(core, profile, state, objective)) {
        return FLIGHT_CONTROL_CORE_INVALID_INPUT;
    }
    if (objective->throttle <
        core->rate_controller->config.integral_activation_throttle) {
        rate_controller_clear_integrals(core->rate_controller);
    }
    core->rate_result = rate_controller_process(
        core->rate_controller,
        core->desired_rate_dps,
        state->angular_rate_dps,
        state->acquired_at_us,
        state->source_sequence,
        true,
        objective->throttle >=
            core->rate_controller->config.integral_activation_throttle,
        &core->rate_output);
    if (core->rate_result == RATE_CONTROLLER_RESULT_NO_NEW_SAMPLE) {
        return FLIGHT_CONTROL_CORE_WAITING_FOR_STATE;
    }
    if (core->rate_result == RATE_CONTROLLER_RESULT_SEEDED) {
        correction[0] = 0.0F;
        correction[1] = 0.0F;
        correction[2] = 0.0F;
        effective_throttle = 0.0F;
    } else if (core->rate_result == RATE_CONTROLLER_RESULT_UPDATED) {
        for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
            correction[axis] = core->rate_output.axis[axis].total;
        }
        effective_throttle = objective->throttle;
    } else {
        return FLIGHT_CONTROL_CORE_INVALID_INPUT;
    }
    output->motor_baseline = profile->armed_idle +
                             effective_throttle *
                                 (1.0F - profile->armed_idle);
    if (!quad_x_mixer_apply_prepared_with_floor(
            &profile->mixer,
            output->motor_baseline,
            profile->armed_idle,
            correction,
            now_us,
            &output->mixer_output)) {
        return FLIGHT_CONTROL_CORE_INVALID_INPUT;
    }
    rate_controller_report_actuator_saturation(
        core->rate_controller, output->mixer_output.saturated);
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        output->desired_rate_dps[axis] = core->desired_rate_dps[axis];
    }
    output->rate_output = core->rate_output;
    output->rate_result = core->rate_result;
    output->mixer_output_valid = true;
    return FLIGHT_CONTROL_CORE_UPDATED;
}
