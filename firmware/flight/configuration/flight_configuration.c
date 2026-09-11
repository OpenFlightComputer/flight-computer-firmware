#include "flight_configuration.h"

#include "flight_configuration_defaults.h"
#include "attitude_estimator.h"
#include "gyro_filter.h"

#include <stddef.h>

void flight_configuration_defaults(flight_configuration_t *configuration)
{
    static const motor_direction_t directions[MOTOR_COMMAND_MOTOR_COUNT] =
        OFC_DEFAULT_MOTOR_DIRECTIONS;
    size_t motor;
    static const control_curve_point_t roll_points[3] =
        OFC_DEFAULT_CONTROL_ROLL_POINTS;
    static const control_curve_point_t pitch_points[3] =
        OFC_DEFAULT_CONTROL_PITCH_POINTS;
    static const control_curve_point_t yaw_points[3] =
        OFC_DEFAULT_CONTROL_YAW_POINTS;
    static const control_curve_point_t throttle_points[2] =
        OFC_DEFAULT_CONTROL_THROTTLE_POINTS;
    size_t point;

    if (configuration == NULL) {
        return;
    }

    *configuration = (flight_configuration_t){
        .schema_version = OFC_DEFAULT_CONFIGURATION_SCHEMA_VERSION,
        .propeller_layout = OFC_DEFAULT_PROPELLER_LAYOUT,
        .mixer = {
            .roll_factor = OFC_DEFAULT_MIXER_ROLL_FACTOR,
            .pitch_factor = OFC_DEFAULT_MIXER_PITCH_FACTOR,
            .yaw_factor = OFC_DEFAULT_MIXER_YAW_FACTOR,
        },
        .control = {
            .roll = {
                .deadband = OFC_DEFAULT_CONTROL_ROLL_DEADBAND,
                .maximum_angle_degrees =
                    OFC_DEFAULT_CONTROL_ROLL_MAXIMUM_ANGLE_DEGREES,
                .maximum_rate_dps =
                    OFC_DEFAULT_CONTROL_ROLL_MAXIMUM_RATE_DPS,
                .curve = {
                    .type = OFC_DEFAULT_CONTROL_CURVE_TYPE,
                    .interpolation =
                        OFC_DEFAULT_CONTROL_CURVE_INTERPOLATION,
                    .point_count = 3U,
                },
            },
            .pitch = {
                .deadband = OFC_DEFAULT_CONTROL_PITCH_DEADBAND,
                .maximum_angle_degrees =
                    OFC_DEFAULT_CONTROL_PITCH_MAXIMUM_ANGLE_DEGREES,
                .maximum_rate_dps =
                    OFC_DEFAULT_CONTROL_PITCH_MAXIMUM_RATE_DPS,
                .curve = {
                    .type = OFC_DEFAULT_CONTROL_CURVE_TYPE,
                    .interpolation =
                        OFC_DEFAULT_CONTROL_CURVE_INTERPOLATION,
                    .point_count = 3U,
                },
            },
            .yaw = {
                .deadband = OFC_DEFAULT_CONTROL_YAW_DEADBAND,
                .maximum_angle_degrees =
                    OFC_DEFAULT_CONTROL_YAW_MAXIMUM_ANGLE_DEGREES,
                .maximum_rate_dps =
                    OFC_DEFAULT_CONTROL_YAW_MAXIMUM_RATE_DPS,
                .curve = {
                    .type = OFC_DEFAULT_CONTROL_CURVE_TYPE,
                    .interpolation =
                        OFC_DEFAULT_CONTROL_CURVE_INTERPOLATION,
                    .point_count = 3U,
                },
            },
            .throttle = {
                .zero_deadband =
                    OFC_DEFAULT_CONTROL_THROTTLE_ZERO_DEADBAND,
                .maximum = OFC_DEFAULT_CONTROL_THROTTLE_MAXIMUM,
                .curve = {
                    .type = OFC_DEFAULT_CONTROL_CURVE_TYPE,
                    .interpolation =
                        OFC_DEFAULT_CONTROL_CURVE_INTERPOLATION,
                    .point_count = 2U,
                },
            },
        },
        .rate_controller = {
            .type = OFC_DEFAULT_RATE_CONTROLLER_TYPE,
            .maximum_gap_us = OFC_DEFAULT_RATE_CONTROLLER_MAXIMUM_GAP_US,
            .axis = {
                {
                    .kp = OFC_DEFAULT_RATE_PID_ROLL_KP,
                    .ki = OFC_DEFAULT_RATE_PID_ROLL_KI,
                    .kd = OFC_DEFAULT_RATE_PID_ROLL_KD,
                    .integral_limit = OFC_DEFAULT_RATE_PID_ROLL_INTEGRAL_LIMIT,
                    .output_limit = OFC_DEFAULT_RATE_PID_ROLL_OUTPUT_LIMIT,
                },
                {
                    .kp = OFC_DEFAULT_RATE_PID_PITCH_KP,
                    .ki = OFC_DEFAULT_RATE_PID_PITCH_KI,
                    .kd = OFC_DEFAULT_RATE_PID_PITCH_KD,
                    .integral_limit = OFC_DEFAULT_RATE_PID_PITCH_INTEGRAL_LIMIT,
                    .output_limit = OFC_DEFAULT_RATE_PID_PITCH_OUTPUT_LIMIT,
                },
                {
                    .kp = OFC_DEFAULT_RATE_PID_YAW_KP,
                    .ki = OFC_DEFAULT_RATE_PID_YAW_KI,
                    .kd = OFC_DEFAULT_RATE_PID_YAW_KD,
                    .integral_limit = OFC_DEFAULT_RATE_PID_YAW_INTEGRAL_LIMIT,
                    .output_limit = OFC_DEFAULT_RATE_PID_YAW_OUTPUT_LIMIT,
                },
            },
        },
        .roll_attitude_controller = {
            .gain_per_s = OFC_DEFAULT_ATTITUDE_ROLL_GAIN_PER_S,
        },
        .pitch_attitude_controller = {
            .gain_per_s = OFC_DEFAULT_ATTITUDE_PITCH_GAIN_PER_S,
        },
        .receiver_failsafe = {
            .stale_after_us = OFC_DEFAULT_FAILSAFE_STALE_AFTER_US,
            .loss_detected_after_us =
                OFC_DEFAULT_FAILSAFE_LOSS_DETECTED_AFTER_US,
            .hold_last_until_us = OFC_DEFAULT_FAILSAFE_HOLD_LAST_UNTIL_US,
            .stage_two_after_us = OFC_DEFAULT_FAILSAFE_STAGE_TWO_AFTER_US,
            .recovery_stable_us = OFC_DEFAULT_FAILSAFE_RECOVERY_STABLE_US,
            .stage_one_roll = OFC_DEFAULT_FAILSAFE_STAGE_ONE_ROLL,
            .stage_one_pitch = OFC_DEFAULT_FAILSAFE_STAGE_ONE_PITCH,
            .stage_one_yaw = OFC_DEFAULT_FAILSAFE_STAGE_ONE_YAW,
            .stage_one_throttle = OFC_DEFAULT_FAILSAFE_STAGE_ONE_THROTTLE,
            .recovery_throttle_maximum =
                OFC_DEFAULT_FAILSAFE_RECOVERY_THROTTLE_MAXIMUM,
        },
        .gyro_calibration = {
            .settling_duration_us =
                OFC_DEFAULT_GYRO_CALIBRATION_SETTLING_DURATION_US,
            .sample_duration_us =
                OFC_DEFAULT_GYRO_CALIBRATION_SAMPLE_DURATION_US,
            .maximum_rate_dps =
                OFC_DEFAULT_GYRO_CALIBRATION_MAXIMUM_RATE_DPS,
            .maximum_standard_deviation_dps =
                OFC_DEFAULT_GYRO_CALIBRATION_MAXIMUM_STANDARD_DEVIATION_DPS,
        },
        .gyro_filter = {
            .type = OFC_DEFAULT_GYRO_FILTER_TYPE,
            .cutoff_hz = OFC_DEFAULT_GYRO_FILTER_CUTOFF_HZ,
        },
        .attitude_estimator = {
            .type = OFC_DEFAULT_ATTITUDE_ESTIMATOR_TYPE,
            .accelerometer_correction_time_constant_s =
                OFC_DEFAULT_ACCELEROMETER_CORRECTION_TIME_CONSTANT_S,
            .maximum_gap_us = OFC_DEFAULT_ATTITUDE_MAXIMUM_GAP_US,
        },
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] = directions[motor];
    }
    for (point = 0U; point < 3U; point++) {
        configuration->control.roll.curve.points[point] = roll_points[point];
        configuration->control.pitch.curve.points[point] =
            pitch_points[point];
        configuration->control.yaw.curve.points[point] = yaw_points[point];
    }
    for (point = 0U; point < 2U; point++) {
        configuration->control.throttle.curve.points[point] =
            throttle_points[point];
    }
}

bool flight_configuration_is_valid(
    const flight_configuration_t *configuration)
{
    return (configuration != NULL) &&
           (configuration->schema_version ==
            OFC_DEFAULT_CONFIGURATION_SCHEMA_VERSION) &&
           (configuration->propeller_layout < PROPELLER_LAYOUT_COUNT) &&
           motor_configuration_is_valid(&configuration->motors) &&
           quad_x_mixer_config_is_valid(&configuration->mixer) &&
           control_input_shaping_config_is_valid(&configuration->control) &&
           roll_attitude_controller_config_is_valid(
               &configuration->roll_attitude_controller) &&
           pitch_attitude_controller_config_is_valid(
               &configuration->pitch_attitude_controller) &&
           rate_controller_config_is_valid(&configuration->rate_controller) &&
           receiver_failsafe_config_is_valid(
               &configuration->receiver_failsafe) &&
           (configuration->receiver_failsafe.stale_after_us <= UINT32_MAX) &&
           (configuration->receiver_failsafe.loss_detected_after_us <=
            UINT32_MAX) &&
           (configuration->receiver_failsafe.hold_last_until_us <=
            UINT32_MAX) &&
           (configuration->receiver_failsafe.stage_two_after_us <=
            UINT32_MAX) &&
           (configuration->receiver_failsafe.recovery_stable_us <=
            UINT32_MAX) &&
           (configuration->gyro_calibration.settling_duration_us <=
            UINT64_C(10000000)) &&
           (configuration->gyro_calibration.sample_duration_us > 0U) &&
           (configuration->gyro_calibration.sample_duration_us <=
            UINT64_C(10000000)) &&
           (configuration->gyro_calibration.maximum_rate_dps > 0.0F) &&
           (configuration->gyro_calibration.maximum_rate_dps <= 2000.0F) &&
           (configuration->gyro_calibration
                .maximum_standard_deviation_dps > 0.0F) &&
           (configuration->gyro_calibration
                .maximum_standard_deviation_dps <= 2000.0F) &&
           (configuration->gyro_filter.type ==
            FLIGHT_GYRO_FILTER_FIRST_ORDER_LOW_PASS) &&
           gyro_filter_config_is_valid(&(gyro_filter_config_t){
               .type = GYRO_FILTER_FIRST_ORDER_LOW_PASS,
               .cutoff_hz = configuration->gyro_filter.cutoff_hz,
           }) &&
           (configuration->attitude_estimator.type ==
            FLIGHT_ATTITUDE_ESTIMATOR_COMPLEMENTARY) &&
           attitude_estimator_config_is_valid(
               &(attitude_estimator_config_t){
                   .type = ATTITUDE_ESTIMATOR_COMPLEMENTARY,
                   .accelerometer_correction_time_constant_s =
                       configuration->attitude_estimator
                           .accelerometer_correction_time_constant_s,
               }) &&
           (configuration->attitude_estimator.maximum_gap_us > 0U) &&
           (configuration->attitude_estimator.maximum_gap_us <= 1000000U);
}
