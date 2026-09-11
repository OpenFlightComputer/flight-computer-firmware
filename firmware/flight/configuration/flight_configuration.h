#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_H

#include "motor_configuration.h"
#include "control_input_shaping.h"
#include "quad_x_mixer.h"
#include "receiver_failsafe.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t settling_duration_us;
    uint64_t sample_duration_us;
    float maximum_rate_dps;
    float maximum_standard_deviation_dps;
} gyro_calibration_configuration_t;

typedef enum {
    FLIGHT_GYRO_FILTER_FIRST_ORDER_LOW_PASS = 0,
    FLIGHT_GYRO_FILTER_TYPE_COUNT,
} flight_gyro_filter_type_t;

typedef enum {
    FLIGHT_ATTITUDE_ESTIMATOR_COMPLEMENTARY = 0,
    FLIGHT_ATTITUDE_ESTIMATOR_TYPE_COUNT,
} flight_attitude_estimator_type_t;

typedef struct {
    flight_gyro_filter_type_t type;
    float cutoff_hz;
} gyro_filter_configuration_t;

typedef struct {
    flight_attitude_estimator_type_t type;
    float accelerometer_correction_time_constant_s;
    uint32_t maximum_gap_us;
} attitude_estimator_configuration_t;

typedef struct {
    uint32_t schema_version;
    propeller_layout_t propeller_layout;
    motor_configuration_t motors;
    quad_x_mixer_config_t mixer;
    control_input_shaping_config_t control;
    receiver_failsafe_config_t receiver_failsafe;
    gyro_calibration_configuration_t gyro_calibration;
    gyro_filter_configuration_t gyro_filter;
    attitude_estimator_configuration_t attitude_estimator;
} flight_configuration_t;

void flight_configuration_defaults(flight_configuration_t *configuration);
bool flight_configuration_is_valid(
    const flight_configuration_t *configuration);

#endif
