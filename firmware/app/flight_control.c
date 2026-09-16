#include "flight_control.h"

#include "motor_control.h"
#include "yaw_attitude_controller.h"

#include <stddef.h>

static void reset_stabilization(flight_control_stabilization_t *stabilization)
{
    if (stabilization == NULL) {
        return;
    }
    if (stabilization->desired_rates != NULL) {
        *stabilization->desired_rates = (flight_control_desired_rates_t){0};
    }
    if ((stabilization->rate_controller != NULL) &&
        (stabilization->rate_output != NULL)) {
        rate_controller_reset(stabilization->rate_controller);
        *stabilization->rate_output = (rate_controller_output_t){0};
        if (stabilization->rate_result != NULL) {
            *stabilization->rate_result =
                (uint32_t)RATE_CONTROLLER_RESULT_DISABLED;
        }
    }
}

static bool prepare_desired_rates(
    const prepared_control_input_shaping_t *control,
    float desired_roll_degrees,
    float desired_pitch_degrees,
    float desired_yaw_rate_dps,
    flight_control_stabilization_t *stabilization)
{
    const attitude_snapshot_t *attitude;
    flight_control_desired_rates_t *desired_rates;
    if ((stabilization == NULL) ||
        (stabilization->roll_controller == NULL) ||
        (stabilization->pitch_controller == NULL) ||
        (stabilization->rate_controller == NULL) ||
        (stabilization->desired_rates == NULL) ||
        (stabilization->rate_output == NULL) ||
        (stabilization->attitude == NULL) ||
        !stabilization->attitude->valid) {
        return false;
    }
    attitude = stabilization->attitude;
    desired_rates = stabilization->desired_rates;
    if (!roll_attitude_controller_update(
            stabilization->roll_controller,
            desired_roll_degrees,
            attitude->roll_degrees,
            control->roll.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_ROLL]) ||
        !pitch_attitude_controller_update(
            stabilization->pitch_controller,
            desired_pitch_degrees,
            attitude->pitch_degrees,
            control->pitch.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_PITCH]) ||
        !yaw_attitude_controller_update(
            desired_yaw_rate_dps,
            control->yaw.maximum_rate_dps,
            &desired_rates->desired_rate_dps[RATE_CONTROLLER_AXIS_YAW])) {
        return false;
    }
    desired_rates->valid = true;
    return true;
}

static void reset_takeoff_leveling(
    flight_control_stabilization_t *stabilization)
{
    if ((stabilization != NULL) &&
        (stabilization->takeoff_leveling != NULL)) {
        takeoff_leveling_reset(stabilization->takeoff_leveling,
                               stabilization->easy_mode);
    }
}

static float easy_mode_motor_baseline(const easy_mode_config_t *config,
                                      float pilot_throttle)
{
    const float idle = easy_mode_armed_idle(config);

    return idle + pilot_throttle * (1.0F - idle);
}

static flight_control_result_t enter_control_failsafe(
    flight_control_stabilization_t *stabilization,
    flight_control_result_t accepted_result)
{
    reset_stabilization(stabilization);
    return motor_control_enter_failsafe() == MOTOR_CONTROL_FAILSAFE_ACCEPTED
               ? accepted_result
               : FLIGHT_CONTROL_FAILSAFE_ERROR;
}

static bool create_stage_one_setpoint(
    const prepared_control_input_shaping_t *control,
    const receiver_control_snapshot_t *requested,
    control_setpoint_t *setpoint)
{
    if ((control == NULL) || (requested == NULL) || !requested->valid ||
        (setpoint == NULL) || (requested->roll < -1.0F) ||
        (requested->roll > 1.0F) || (requested->pitch < -1.0F) ||
        (requested->pitch > 1.0F) || (requested->yaw < -1.0F) ||
        (requested->yaw > 1.0F) || (requested->throttle < 0.0F) ||
        (requested->throttle > 1.0F)) {
        return false;
    }
    *setpoint = (control_setpoint_t){
        .roll_normalized = requested->roll,
        .pitch_normalized = requested->pitch,
        .yaw_normalized = requested->yaw,
        .throttle = requested->throttle,
        .desired_roll_degrees =
            requested->roll * control->roll.maximum_angle_degrees,
        .desired_pitch_degrees =
            requested->pitch * control->pitch.maximum_angle_degrees,
        .desired_yaw_rate_dps =
            requested->yaw * control->yaw.maximum_rate_dps,
        .valid = true,
    };
    return true;
}

flight_control_result_t flight_control_process_receiver(
    const prepared_control_input_shaping_t *control,
    const prepared_quad_x_mixer_t *mixer,
    const receiver_failsafe_decision_t *decision,
    flight_control_stabilization_t *stabilization,
    flight_control_output_t *output,
    uint64_t now_us)
{
    quad_x_mixer_output_t mixer_output;
    float correction[RATE_CONTROLLER_AXIS_COUNT];
    rate_controller_result_t rate_result;
    control_setpoint_t setpoint;

    if (output != NULL) {
        *output = (flight_control_output_t){0};
    }
    if ((control == NULL) || (mixer == NULL) || (decision == NULL) ||
        (motor_control_active_source() != MOTOR_CONTROL_SOURCE_RECEIVER) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_NONE)) {
        reset_stabilization(stabilization);
        reset_takeoff_leveling(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STOP) {
        reset_stabilization(stabilization);
        reset_takeoff_leveling(stabilization);
        return motor_control_enter_failsafe() ==
                       MOTOR_CONTROL_FAILSAFE_ACCEPTED
                   ? FLIGHT_CONTROL_FAILSAFE_ENTERED
                   : FLIGHT_CONTROL_FAILSAFE_ERROR;
    }
    if ((decision->action == RECEIVER_FAILSAFE_ACTION_LIVE) ||
        (decision->action == RECEIVER_FAILSAFE_ACTION_HOLD_LAST)) {
        if (!control_input_shaping_apply(control,
                                         &decision->requested_control,
                                         &setpoint)) {
            return enter_control_failsafe(
                stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
        }
    } else if ((decision->action != RECEIVER_FAILSAFE_ACTION_STAGE_ONE) ||
               !create_stage_one_setpoint(control,
                                          &decision->requested_control,
                                          &setpoint)) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    if (output != NULL) {
        output->setpoint = setpoint;
    }
    if ((stabilization == NULL) ||
        !easy_mode_config_is_valid(stabilization->easy_mode) ||
        (stabilization->takeoff_leveling == NULL) ||
        !stabilization->takeoff_leveling->initialized) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    if (setpoint.throttle == 0.0F) {
        static const float zero_correction[RATE_CONTROLLER_AXIS_COUNT];
        const float idle = easy_mode_armed_idle(stabilization->easy_mode);

        reset_stabilization(stabilization);
        if ((stabilization->attitude != NULL) &&
            stabilization->attitude->valid) {
            (void)takeoff_leveling_apply(
                stabilization->takeoff_leveling,
                stabilization->easy_mode,
                0.0F,
                setpoint.desired_roll_degrees,
                setpoint.desired_pitch_degrees,
                stabilization->attitude->roll_degrees,
                stabilization->attitude->pitch_degrees,
                stabilization->attitude->acquired_at_us,
                &stabilization->takeoff_leveling->effective_roll_degrees,
                &stabilization->takeoff_leveling->effective_pitch_degrees);
        }
        if (!quad_x_mixer_apply_prepared_with_floor(
                mixer, idle, idle, zero_correction,
                now_us, &mixer_output)) {
            return enter_control_failsafe(
                stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
        }
        if (output != NULL) {
            output->mixer_output = mixer_output;
            output->mixer_output_valid = true;
            output->effective_roll_degrees =
                stabilization->takeoff_leveling->effective_roll_degrees;
            output->effective_pitch_degrees =
                stabilization->takeoff_leveling->effective_pitch_degrees;
            output->motor_baseline = idle;
            output->takeoff_leveling_state =
                stabilization->takeoff_leveling->state;
        }
        return motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                    &mixer_output.command) ==
                       MOTOR_CONTROL_SUBMIT_ACCEPTED
                   ? FLIGHT_CONTROL_SUBMITTED
                   : FLIGHT_CONTROL_SUBMIT_ERROR;
    }
    if (stabilization->imu_freshness == IMU_FRESHNESS_STALE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (stabilization->imu_freshness != IMU_FRESHNESS_FRESH) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_IMU_FAILSAFE_ENTERED);
    }
    if (decision->action == RECEIVER_FAILSAFE_ACTION_STAGE_ONE) {
        takeoff_leveling_complete(stabilization->takeoff_leveling,
                                  setpoint.desired_roll_degrees,
                                  setpoint.desired_pitch_degrees);
    } else if (!takeoff_leveling_apply(
                   stabilization->takeoff_leveling,
                   stabilization->easy_mode,
                   setpoint.throttle,
                   setpoint.desired_roll_degrees,
                   setpoint.desired_pitch_degrees,
                   stabilization->attitude->roll_degrees,
                   stabilization->attitude->pitch_degrees,
                   stabilization->attitude->acquired_at_us,
                   &stabilization->takeoff_leveling->effective_roll_degrees,
                   &stabilization->takeoff_leveling->effective_pitch_degrees)) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    if (!prepare_desired_rates(
            control,
            stabilization->takeoff_leveling->effective_roll_degrees,
            stabilization->takeoff_leveling->effective_pitch_degrees,
            setpoint.desired_yaw_rate_dps,
            stabilization)) {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_IMU_FAILSAFE_ENTERED);
    }
    if (setpoint.throttle < stabilization->rate_controller->config
                                .integral_activation_throttle) {
        rate_controller_clear_integrals(stabilization->rate_controller);
    }
    rate_result = rate_controller_process(
        stabilization->rate_controller,
        stabilization->desired_rates->desired_rate_dps,
        stabilization->attitude->filtered_gyroscope_dps,
        stabilization->attitude->acquired_at_us,
        stabilization->attitude->source_sequence,
        true,
        setpoint.throttle >= stabilization->rate_controller->config
                                 .integral_activation_throttle,
        stabilization->rate_output);
    if (stabilization->rate_result != NULL) {
        *stabilization->rate_result = (uint32_t)rate_result;
    }
    if (rate_result == RATE_CONTROLLER_RESULT_NO_NEW_SAMPLE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (rate_result == RATE_CONTROLLER_RESULT_SEEDED) {
        correction[RATE_CONTROLLER_AXIS_ROLL] = 0.0F;
        correction[RATE_CONTROLLER_AXIS_PITCH] = 0.0F;
        correction[RATE_CONTROLLER_AXIS_YAW] = 0.0F;
        setpoint.throttle = 0.0F;
    } else if (rate_result == RATE_CONTROLLER_RESULT_UPDATED) {
        size_t axis;

        for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
            correction[axis] = stabilization->rate_output->axis[axis].total;
        }
    } else {
        return enter_control_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    {
        float motor_baseline = easy_mode_motor_baseline(
            stabilization->easy_mode, setpoint.throttle);
        const float idle = easy_mode_armed_idle(stabilization->easy_mode);

        if (!quad_x_mixer_apply_prepared_with_floor(
                mixer, motor_baseline, idle, correction,
                now_us, &mixer_output)) {
            return enter_control_failsafe(
                stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
        }
        if (output != NULL) {
            output->motor_baseline = motor_baseline;
        }
    }
    rate_controller_report_actuator_saturation(
        stabilization->rate_controller, mixer_output.saturated);
    if (output != NULL) {
        output->setpoint = setpoint;
        output->mixer_output = mixer_output;
        output->mixer_output_valid = true;
        output->effective_roll_degrees =
            stabilization->takeoff_leveling->effective_roll_degrees;
        output->effective_pitch_degrees =
            stabilization->takeoff_leveling->effective_pitch_degrees;
        output->takeoff_leveling_state =
            stabilization->takeoff_leveling->state;
    }
    return motor_control_submit(MOTOR_CONTROL_SOURCE_RECEIVER,
                                &mixer_output.command) ==
                   MOTOR_CONTROL_SUBMIT_ACCEPTED
               ? FLIGHT_CONTROL_SUBMITTED
               : FLIGHT_CONTROL_SUBMIT_ERROR;
}

flight_control_result_t flight_control_recover_receiver(
    receiver_failsafe_t *failsafe)
{
    if ((failsafe == NULL) || !failsafe->initialized ||
        !failsafe->stage_two_latched || !failsafe->recovery_ready) {
        return FLIGHT_CONTROL_RECOVERY_ERROR;
    }
    if (motor_control_recover_to_disarmed() !=
        MOTOR_CONTROL_RECOVERY_ACCEPTED) {
        return FLIGHT_CONTROL_RECOVERY_ERROR;
    }
    return receiver_failsafe_release_stage_two(failsafe)
               ? FLIGHT_CONTROL_RECOVERED
               : FLIGHT_CONTROL_RECOVERY_ERROR;
}
