#include "board_flight_configuration_storage.h"

#include "board.h"

#include <stdint.h>

#define FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(4)
#define PREVIOUS_FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(3)
#define OLDER_FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(2)
#define OLDEST_FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(1)
#define LEGACY_MOTOR_CONFIGURATION_PAYLOAD_VERSION UINT32_C(1)

typedef struct {
    uint8_t type;
    uint8_t interpolation;
    uint8_t point_count;
    uint8_t reserved;
    float points[CONTROL_CURVE_MAXIMUM_POINTS][2];
} control_curve_payload_t;

typedef struct {
    uint32_t version;
    uint32_t schema_version;
    uint32_t timing_us[5];
    float mixer_factors[3];
    float failsafe_controls[5];
    uint32_t gyro_timing_us[2];
    float gyro_thresholds_dps[2];
    float processing_parameters[2];
    uint32_t attitude_maximum_gap_us;
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t gyro_filter_type;
    uint8_t attitude_estimator_type;
    uint8_t reserved[1];
    float control_axis_parameters[3][3];
    float throttle_parameters[2];
    control_curve_payload_t control_curves[4];
} flight_configuration_payload_t;

typedef struct {
    uint32_t version;
    uint32_t schema_version;
    uint32_t timing_us[5];
    float mixer_factors[3];
    float failsafe_controls[5];
    uint32_t gyro_timing_us[2];
    float gyro_thresholds_dps[2];
    float processing_parameters[2];
    uint32_t attitude_maximum_gap_us;
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t gyro_filter_type;
    uint8_t attitude_estimator_type;
    uint8_t reserved[1];
} previous_flight_configuration_payload_t;

typedef struct {
    uint32_t version;
    uint32_t schema_version;
    uint32_t timing_us[5];
    float mixer_factors[3];
    float failsafe_controls[5];
    uint32_t gyro_timing_us[2];
    float gyro_thresholds_dps[2];
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t reserved[3];
} older_flight_configuration_payload_t;

typedef struct {
    uint32_t version;
    uint32_t schema_version;
    uint64_t timing_us[5];
    float mixer_factors[3];
    float failsafe_controls[5];
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t reserved[3];
} oldest_flight_configuration_payload_t;

typedef struct {
    uint32_t version;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
} legacy_motor_configuration_payload_t;

_Static_assert(sizeof(flight_configuration_payload_t) == 412U,
               "Flight configuration payload format changed");
_Static_assert(sizeof(previous_flight_configuration_payload_t) == 96U,
               "Previous flight configuration payload format changed");
_Static_assert(sizeof(older_flight_configuration_payload_t) == 84U,
               "Older flight configuration payload format changed");
_Static_assert(sizeof(oldest_flight_configuration_payload_t) == 88U,
               "Oldest flight configuration payload format changed");

static void encode(const flight_configuration_t *configuration,
                   flight_configuration_payload_t *payload)
{
    const control_curve_config_t *curves[4] = {
        &configuration->control.roll.curve,
        &configuration->control.pitch.curve,
        &configuration->control.yaw.curve,
        &configuration->control.throttle.curve,
    };
    size_t curve;
    size_t point;
    size_t motor;

    *payload = (flight_configuration_payload_t){
        .version = FLIGHT_CONFIGURATION_PAYLOAD_VERSION,
        .schema_version = configuration->schema_version,
        .timing_us = {
            (uint32_t)configuration->receiver_failsafe.stale_after_us,
            (uint32_t)configuration->receiver_failsafe.loss_detected_after_us,
            (uint32_t)configuration->receiver_failsafe.hold_last_until_us,
            (uint32_t)configuration->receiver_failsafe.stage_two_after_us,
            (uint32_t)configuration->receiver_failsafe.recovery_stable_us,
        },
        .mixer_factors = {
            configuration->mixer.roll_factor,
            configuration->mixer.pitch_factor,
            configuration->mixer.yaw_factor,
        },
        .failsafe_controls = {
            configuration->receiver_failsafe.stage_one_roll,
            configuration->receiver_failsafe.stage_one_pitch,
            configuration->receiver_failsafe.stage_one_yaw,
            configuration->receiver_failsafe.stage_one_throttle,
            configuration->receiver_failsafe.recovery_throttle_maximum,
        },
        .propeller_layout = (uint8_t)configuration->propeller_layout,
        .gyro_timing_us = {
            (uint32_t)configuration->gyro_calibration.settling_duration_us,
            (uint32_t)configuration->gyro_calibration.sample_duration_us,
        },
        .gyro_thresholds_dps = {
            configuration->gyro_calibration.maximum_rate_dps,
            configuration->gyro_calibration.maximum_standard_deviation_dps,
        },
        .processing_parameters = {
            configuration->gyro_filter.cutoff_hz,
            configuration->attitude_estimator
                .accelerometer_correction_time_constant_s,
        },
        .attitude_maximum_gap_us =
            configuration->attitude_estimator.maximum_gap_us,
        .gyro_filter_type = (uint8_t)configuration->gyro_filter.type,
        .attitude_estimator_type =
            (uint8_t)configuration->attitude_estimator.type,
        .control_axis_parameters = {
            {configuration->control.roll.deadband,
             configuration->control.roll.maximum_angle_degrees,
             configuration->control.roll.maximum_rate_dps},
            {configuration->control.pitch.deadband,
             configuration->control.pitch.maximum_angle_degrees,
             configuration->control.pitch.maximum_rate_dps},
            {configuration->control.yaw.deadband,
             configuration->control.yaw.maximum_angle_degrees,
             configuration->control.yaw.maximum_rate_dps},
        },
        .throttle_parameters = {
            configuration->control.throttle.zero_deadband,
            configuration->control.throttle.maximum,
        },
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        payload->directions[motor] =
            (uint8_t)configuration->motors.direction[motor];
    }
    for (curve = 0U; curve < 4U; curve++) {
        payload->control_curves[curve].type = (uint8_t)curves[curve]->type;
        payload->control_curves[curve].interpolation =
            (uint8_t)curves[curve]->interpolation;
        payload->control_curves[curve].point_count = curves[curve]->point_count;
        for (point = 0U; point < curves[curve]->point_count; point++) {
            payload->control_curves[curve].points[point][0] =
                curves[curve]->points[point].input;
            payload->control_curves[curve].points[point][1] =
                curves[curve]->points[point].output;
        }
    }
}

static bool decode(const flight_configuration_payload_t *payload,
                   flight_configuration_t *configuration)
{
    control_curve_config_t *curves[4];
    size_t curve;
    size_t point;
    size_t motor;

    if (payload->version != FLIGHT_CONFIGURATION_PAYLOAD_VERSION) {
        return false;
    }
    *configuration = (flight_configuration_t){
        .schema_version = payload->schema_version,
        .propeller_layout = (propeller_layout_t)payload->propeller_layout,
        .mixer = {
            .roll_factor = payload->mixer_factors[0],
            .pitch_factor = payload->mixer_factors[1],
            .yaw_factor = payload->mixer_factors[2],
        },
        .receiver_failsafe = {
            .stale_after_us = payload->timing_us[0],
            .loss_detected_after_us = payload->timing_us[1],
            .hold_last_until_us = payload->timing_us[2],
            .stage_two_after_us = payload->timing_us[3],
            .recovery_stable_us = payload->timing_us[4],
            .stage_one_roll = payload->failsafe_controls[0],
            .stage_one_pitch = payload->failsafe_controls[1],
            .stage_one_yaw = payload->failsafe_controls[2],
            .stage_one_throttle = payload->failsafe_controls[3],
            .recovery_throttle_maximum = payload->failsafe_controls[4],
        },
        .gyro_calibration = {
            .settling_duration_us = payload->gyro_timing_us[0],
            .sample_duration_us = payload->gyro_timing_us[1],
            .maximum_rate_dps = payload->gyro_thresholds_dps[0],
            .maximum_standard_deviation_dps =
                payload->gyro_thresholds_dps[1],
        },
        .gyro_filter = {
            .type = (flight_gyro_filter_type_t)payload->gyro_filter_type,
            .cutoff_hz = payload->processing_parameters[0],
        },
        .attitude_estimator = {
            .type = (flight_attitude_estimator_type_t)
                payload->attitude_estimator_type,
            .accelerometer_correction_time_constant_s =
                payload->processing_parameters[1],
            .maximum_gap_us = payload->attitude_maximum_gap_us,
        },
        .control = {
            .roll = {
                .deadband = payload->control_axis_parameters[0][0],
                .maximum_angle_degrees =
                    payload->control_axis_parameters[0][1],
                .maximum_rate_dps = payload->control_axis_parameters[0][2],
            },
            .pitch = {
                .deadband = payload->control_axis_parameters[1][0],
                .maximum_angle_degrees =
                    payload->control_axis_parameters[1][1],
                .maximum_rate_dps = payload->control_axis_parameters[1][2],
            },
            .yaw = {
                .deadband = payload->control_axis_parameters[2][0],
                .maximum_angle_degrees =
                    payload->control_axis_parameters[2][1],
                .maximum_rate_dps = payload->control_axis_parameters[2][2],
            },
            .throttle = {
                .zero_deadband = payload->throttle_parameters[0],
                .maximum = payload->throttle_parameters[1],
            },
        },
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] =
            (motor_direction_t)payload->directions[motor];
    }
    curves[0] = &configuration->control.roll.curve;
    curves[1] = &configuration->control.pitch.curve;
    curves[2] = &configuration->control.yaw.curve;
    curves[3] = &configuration->control.throttle.curve;
    for (curve = 0U; curve < 4U; curve++) {
        curves[curve]->type =
            (control_curve_type_t)payload->control_curves[curve].type;
        curves[curve]->interpolation = (control_curve_interpolation_t)
            payload->control_curves[curve].interpolation;
        curves[curve]->point_count =
            payload->control_curves[curve].point_count;
        if (curves[curve]->point_count > CONTROL_CURVE_MAXIMUM_POINTS) {
            return false;
        }
        for (point = 0U; point < curves[curve]->point_count; point++) {
            curves[curve]->points[point] = (control_curve_point_t){
                .input = payload->control_curves[curve].points[point][0],
                .output = payload->control_curves[curve].points[point][1],
            };
        }
    }
    return flight_configuration_is_valid(configuration);
}

static bool decode_previous(
    const previous_flight_configuration_payload_t *payload,
    flight_configuration_t *configuration)
{
    size_t motor;

    if ((payload->version != PREVIOUS_FLIGHT_CONFIGURATION_PAYLOAD_VERSION) ||
        (payload->schema_version != 3U)) {
        return false;
    }
    flight_configuration_defaults(configuration);
    configuration->propeller_layout =
        (propeller_layout_t)payload->propeller_layout;
    configuration->mixer = (quad_x_mixer_config_t){
        .roll_factor = payload->mixer_factors[0],
        .pitch_factor = payload->mixer_factors[1],
        .yaw_factor = payload->mixer_factors[2],
    };
    configuration->receiver_failsafe = (receiver_failsafe_config_t){
        .stale_after_us = payload->timing_us[0],
        .loss_detected_after_us = payload->timing_us[1],
        .hold_last_until_us = payload->timing_us[2],
        .stage_two_after_us = payload->timing_us[3],
        .recovery_stable_us = payload->timing_us[4],
        .stage_one_roll = payload->failsafe_controls[0],
        .stage_one_pitch = payload->failsafe_controls[1],
        .stage_one_yaw = payload->failsafe_controls[2],
        .stage_one_throttle = payload->failsafe_controls[3],
        .recovery_throttle_maximum = payload->failsafe_controls[4],
    };
    configuration->gyro_calibration = (gyro_calibration_configuration_t){
        .settling_duration_us = payload->gyro_timing_us[0],
        .sample_duration_us = payload->gyro_timing_us[1],
        .maximum_rate_dps = payload->gyro_thresholds_dps[0],
        .maximum_standard_deviation_dps = payload->gyro_thresholds_dps[1],
    };
    configuration->gyro_filter = (gyro_filter_configuration_t){
        .type = (flight_gyro_filter_type_t)payload->gyro_filter_type,
        .cutoff_hz = payload->processing_parameters[0],
    };
    configuration->attitude_estimator =
        (attitude_estimator_configuration_t){
            .type = (flight_attitude_estimator_type_t)
                payload->attitude_estimator_type,
            .accelerometer_correction_time_constant_s =
                payload->processing_parameters[1],
            .maximum_gap_us = payload->attitude_maximum_gap_us,
        };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] =
            (motor_direction_t)payload->directions[motor];
    }
    return flight_configuration_is_valid(configuration);
}

static bool decode_older(
    const older_flight_configuration_payload_t *payload,
    flight_configuration_t *configuration)
{
    size_t motor;

    if ((payload->version != OLDER_FLIGHT_CONFIGURATION_PAYLOAD_VERSION) ||
        (payload->schema_version != 2U)) {
        return false;
    }
    flight_configuration_defaults(configuration);
    configuration->propeller_layout =
        (propeller_layout_t)payload->propeller_layout;
    configuration->mixer = (quad_x_mixer_config_t){
        .roll_factor = payload->mixer_factors[0],
        .pitch_factor = payload->mixer_factors[1],
        .yaw_factor = payload->mixer_factors[2],
    };
    configuration->receiver_failsafe = (receiver_failsafe_config_t){
        .stale_after_us = payload->timing_us[0],
        .loss_detected_after_us = payload->timing_us[1],
        .hold_last_until_us = payload->timing_us[2],
        .stage_two_after_us = payload->timing_us[3],
        .recovery_stable_us = payload->timing_us[4],
        .stage_one_roll = payload->failsafe_controls[0],
        .stage_one_pitch = payload->failsafe_controls[1],
        .stage_one_yaw = payload->failsafe_controls[2],
        .stage_one_throttle = payload->failsafe_controls[3],
        .recovery_throttle_maximum = payload->failsafe_controls[4],
    };
    configuration->gyro_calibration = (gyro_calibration_configuration_t){
        .settling_duration_us = payload->gyro_timing_us[0],
        .sample_duration_us = payload->gyro_timing_us[1],
        .maximum_rate_dps = payload->gyro_thresholds_dps[0],
        .maximum_standard_deviation_dps = payload->gyro_thresholds_dps[1],
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] =
            (motor_direction_t)payload->directions[motor];
    }
    return flight_configuration_is_valid(configuration);
}

static bool decode_oldest(
    const oldest_flight_configuration_payload_t *payload,
    flight_configuration_t *configuration)
{
    size_t motor;

    if ((payload->version != OLDEST_FLIGHT_CONFIGURATION_PAYLOAD_VERSION) ||
        (payload->schema_version != 1U)) {
        return false;
    }
    flight_configuration_defaults(configuration);
    configuration->propeller_layout =
        (propeller_layout_t)payload->propeller_layout;
    configuration->mixer = (quad_x_mixer_config_t){
        .roll_factor = payload->mixer_factors[0],
        .pitch_factor = payload->mixer_factors[1],
        .yaw_factor = payload->mixer_factors[2],
    };
    configuration->receiver_failsafe = (receiver_failsafe_config_t){
        .stale_after_us = payload->timing_us[0],
        .loss_detected_after_us = payload->timing_us[1],
        .hold_last_until_us = payload->timing_us[2],
        .stage_two_after_us = payload->timing_us[3],
        .recovery_stable_us = payload->timing_us[4],
        .stage_one_roll = payload->failsafe_controls[0],
        .stage_one_pitch = payload->failsafe_controls[1],
        .stage_one_yaw = payload->failsafe_controls[2],
        .stage_one_throttle = payload->failsafe_controls[3],
        .recovery_throttle_maximum = payload->failsafe_controls[4],
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] =
            (motor_direction_t)payload->directions[motor];
    }
    return flight_configuration_is_valid(configuration);
}

static flight_configuration_load_result_t load_configuration(
    void *context,
    flight_configuration_t *configuration)
{
    flight_configuration_payload_t payload;
    board_persistent_storage_read_result_t result;

    (void)context;
    if (configuration == NULL) {
        return FLIGHT_CONFIGURATION_LOAD_ERROR;
    }
    result = board_persistent_storage_read(&payload, sizeof(payload));
    if (result == BOARD_PERSISTENT_STORAGE_READ_EMPTY) {
        return FLIGHT_CONFIGURATION_LOAD_EMPTY;
    }
    if (result == BOARD_PERSISTENT_STORAGE_READ_OK) {
        return decode(&payload, configuration)
                   ? FLIGHT_CONFIGURATION_LOAD_OK
                   : FLIGHT_CONFIGURATION_LOAD_ERROR;
    }
    {
        previous_flight_configuration_payload_t previous;

        if ((board_persistent_storage_read(&previous, sizeof(previous)) ==
             BOARD_PERSISTENT_STORAGE_READ_OK) &&
            decode_previous(&previous, configuration)) {
            return FLIGHT_CONFIGURATION_LOAD_OK;
        }
    }
    {
        older_flight_configuration_payload_t older;

        if ((board_persistent_storage_read(&older, sizeof(older)) ==
             BOARD_PERSISTENT_STORAGE_READ_OK) &&
            decode_older(&older, configuration)) {
            return FLIGHT_CONFIGURATION_LOAD_OK;
        }
    }
    {
        oldest_flight_configuration_payload_t oldest;

        if ((board_persistent_storage_read(&oldest, sizeof(oldest)) ==
             BOARD_PERSISTENT_STORAGE_READ_OK) &&
            decode_oldest(&oldest, configuration)) {
            return FLIGHT_CONFIGURATION_LOAD_OK;
        }
    }
    {
        legacy_motor_configuration_payload_t legacy;
        size_t motor;

        if ((board_persistent_storage_read_legacy(&legacy, sizeof(legacy)) !=
             BOARD_PERSISTENT_STORAGE_READ_OK) ||
            (legacy.version !=
             LEGACY_MOTOR_CONFIGURATION_PAYLOAD_VERSION)) {
            return FLIGHT_CONFIGURATION_LOAD_ERROR;
        }
        flight_configuration_defaults(configuration);
        for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            configuration->motors.direction[motor] =
                (motor_direction_t)legacy.directions[motor];
        }
        return flight_configuration_is_valid(configuration)
                   ? FLIGHT_CONFIGURATION_LOAD_OK
                   : FLIGHT_CONFIGURATION_LOAD_ERROR;
    }
}

static flight_configuration_save_result_t save_configuration(
    void *context,
    const flight_configuration_t *configuration)
{
    flight_configuration_payload_t payload;

    (void)context;
    if (!flight_configuration_is_valid(configuration)) {
        return FLIGHT_CONFIGURATION_SAVE_ERROR;
    }
    encode(configuration, &payload);
    return board_persistent_storage_write(&payload, sizeof(payload)) ==
                   BOARD_PERSISTENT_STORAGE_WRITE_OK
               ? FLIGHT_CONFIGURATION_SAVE_OK
               : FLIGHT_CONFIGURATION_SAVE_ERROR;
}

static flight_configuration_clear_result_t clear_configuration(void *context)
{
    (void)context;
    return board_persistent_storage_clear() ==
                   BOARD_PERSISTENT_STORAGE_CLEAR_OK
               ? FLIGHT_CONFIGURATION_CLEAR_OK
               : FLIGHT_CONFIGURATION_CLEAR_ERROR;
}

flight_configuration_storage_t board_flight_configuration_storage(void)
{
    return (flight_configuration_storage_t){
        .load = load_configuration,
        .save = save_configuration,
        .clear = clear_configuration,
    };
}
