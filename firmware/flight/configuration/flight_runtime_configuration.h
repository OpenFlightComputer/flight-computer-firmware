#ifndef OPENFLIGHTCOMPUTER_FLIGHT_RUNTIME_CONFIGURATION_H
#define OPENFLIGHTCOMPUTER_FLIGHT_RUNTIME_CONFIGURATION_H

#include "flight_configuration.h"
#include "flight_control_core.h"
#include "imu_processing_pipeline.h"
#include "level_calibration.h"
#include "receiver_freshness.h"

#include <stdbool.h>

typedef struct {
    prepared_control_input_shaping_t control_input;
    prepared_quad_x_mixer_t mixer;
    prepared_control_profile_t control_profile;
    rate_controller_t rate_controller;
    receiver_freshness_config_t receiver_freshness;
    imu_processing_config_t imu_processing;
    level_calibration_config_t level_calibration;
    bool level_calibrated;
    float level_roll_trim_degrees;
    float level_pitch_trim_degrees;
    bool initialized;
} flight_runtime_configuration_t;

bool flight_runtime_configuration_prepare(
    const flight_configuration_t *configuration,
    float acceleration_counts_per_g,
    float gyroscope_counts_per_dps,
    flight_runtime_configuration_t *runtime);

#endif
