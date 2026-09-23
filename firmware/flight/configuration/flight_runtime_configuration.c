#include "flight_runtime_configuration.h"

#include <math.h>
#include <stddef.h>

static bool positive_finite(float value)
{
    return isfinite(value) && (value > 0.0F);
}

bool flight_runtime_configuration_prepare(
    const flight_configuration_t *configuration,
    float acceleration_counts_per_g,
    float gyroscope_counts_per_dps,
    flight_runtime_configuration_t *runtime)
{
    flight_runtime_configuration_t prepared;
    float maximum_rate_dps[RATE_CONTROLLER_AXIS_COUNT];

    if ((configuration == NULL) || (runtime == NULL) ||
        !flight_configuration_is_valid(configuration) ||
        !positive_finite(acceleration_counts_per_g) ||
        !positive_finite(gyroscope_counts_per_dps)) {
        return false;
    }
    maximum_rate_dps[RATE_CONTROLLER_AXIS_ROLL] =
        configuration->control.roll.maximum_rate_dps;
    maximum_rate_dps[RATE_CONTROLLER_AXIS_PITCH] =
        configuration->control.pitch.maximum_rate_dps;
    maximum_rate_dps[RATE_CONTROLLER_AXIS_YAW] =
        configuration->control.yaw.maximum_rate_dps;

    prepared = (flight_runtime_configuration_t){
        .receiver_freshness = {
            .fresh_through_us =
                configuration->receiver_failsafe.stale_after_us,
            .lost_after_us =
                configuration->receiver_failsafe.loss_detected_after_us,
        },
        .imu_processing = {
            .acceleration_filter = {
                .type = ACCELERATION_FILTER_FIRST_ORDER_LOW_PASS,
                .cutoff_hz = configuration->acceleration_filter.cutoff_hz,
            },
            .gyro_filter = {
                .type = GYRO_FILTER_FIRST_ORDER_LOW_PASS,
                .cutoff_hz = configuration->gyro_filter.cutoff_hz,
            },
            .attitude_estimator = {
                .type = ATTITUDE_ESTIMATOR_COMPLEMENTARY,
                .accelerometer_correction_time_constant_s =
                    configuration->attitude_estimator
                        .accelerometer_correction_time_constant_s,
            },
            .maximum_gap_us =
                configuration->attitude_estimator.maximum_gap_us,
            .acceleration_counts_per_g = acceleration_counts_per_g,
            .gyroscope_counts_per_dps = gyroscope_counts_per_dps,
            .level_roll_trim_degrees =
                configuration->level_calibration.calibrated
                    ? configuration->level_calibration.roll_trim_degrees
                    : 0.0F,
            .level_pitch_trim_degrees =
                configuration->level_calibration.calibrated
                    ? configuration->level_calibration.pitch_trim_degrees
                    : 0.0F,
        },
        .level_calibration = {
            .sample_duration_us =
                configuration->level_calibration.sample_duration_us,
            .minimum_sample_count = 100U,
            .maximum_rate_dps =
                configuration->gyro_calibration.maximum_rate_dps,
            .maximum_acceleration_standard_deviation_g =
                configuration->level_calibration
                    .maximum_acceleration_standard_deviation_g,
            .maximum_acceleration_magnitude_error_g =
                configuration->level_calibration
                    .maximum_acceleration_magnitude_error_g,
            .maximum_trim_degrees =
                configuration->level_calibration.maximum_trim_degrees,
            .acceleration_counts_per_g = acceleration_counts_per_g,
            .gyroscope_counts_per_dps = gyroscope_counts_per_dps,
        },
        .level_calibrated = configuration->level_calibration.calibrated,
        .level_roll_trim_degrees =
            configuration->level_calibration.roll_trim_degrees,
        .level_pitch_trim_degrees =
            configuration->level_calibration.pitch_trim_degrees,
    };

    if (!control_input_shaping_prepare(&configuration->control,
                                       &prepared.control_input) ||
        !quad_x_mixer_prepare(configuration->propeller_layout,
                              &prepared.mixer) ||
        !flight_control_profile_prepare(
            &configuration->roll_attitude_controller,
            &configuration->pitch_attitude_controller,
            maximum_rate_dps,
            &prepared.mixer,
            easy_mode_armed_idle(&configuration->easy_mode),
            &prepared.control_profile) ||
        !rate_controller_initialize(&prepared.rate_controller,
                                    &configuration->rate_controller) ||
        !imu_processing_config_is_valid(&prepared.imu_processing) ||
        !level_calibration_config_is_valid(&prepared.level_calibration)) {
        return false;
    }

    prepared.initialized = true;
    *runtime = prepared;
    return true;
}
