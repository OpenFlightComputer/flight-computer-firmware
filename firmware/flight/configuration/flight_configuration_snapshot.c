#include "flight_configuration_snapshot.h"

#include <stddef.h>
#include <string.h>

#define FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(10)

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
    float level_thresholds[3];
    float failsafe_controls[5];
    uint32_t gyro_timing_us[2];
    float gyro_thresholds_dps[2];
    float processing_parameters[2];
    uint32_t attitude_maximum_gap_us;
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t gyro_filter_type;
    uint8_t attitude_estimator_type;
    uint8_t level_calibrated;
    float control_axis_parameters[3][3];
    float throttle_parameters[2];
    control_curve_payload_t control_curves[4];
    uint32_t rate_controller_maximum_gap_us;
    uint8_t rate_controller_type;
    uint8_t acceleration_filter_type;
    uint8_t behavior_id;
    uint8_t behavior_settings_version;
    float rate_pid_parameters[RATE_CONTROLLER_AXIS_COUNT][5];
    float attitude_angle_gain_per_s[2];
    uint32_t level_sample_duration_us;
    float level_trim_degrees[2];
    float acceleration_filter_cutoff_hz;
    float integral_activation_throttle;
    uint32_t easy_mode_packed;
} flight_configuration_payload_t;

_Static_assert(sizeof(flight_configuration_payload_t) ==
                   FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY,
               "Flight configuration snapshot format changed");

static void encode_payload(const flight_configuration_t *configuration,
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
        .level_thresholds = {
            configuration->level_calibration
                .maximum_acceleration_standard_deviation_g,
            configuration->level_calibration
                .maximum_acceleration_magnitude_error_g,
            configuration->level_calibration.maximum_trim_degrees,
        },
        .failsafe_controls = {
            configuration->receiver_failsafe.stage_one_roll,
            configuration->receiver_failsafe.stage_one_pitch,
            configuration->receiver_failsafe.stage_one_yaw,
            configuration->receiver_failsafe.stage_one_throttle,
            configuration->receiver_failsafe.recovery_throttle_maximum,
        },
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
        .propeller_layout = (uint8_t)configuration->propeller_layout,
        .gyro_filter_type = (uint8_t)configuration->gyro_filter.type,
        .attitude_estimator_type =
            (uint8_t)configuration->attitude_estimator.type,
        .level_calibrated = configuration->level_calibration.calibrated
                                ? UINT8_C(1)
                                : UINT8_C(0),
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
        .rate_controller_maximum_gap_us =
            configuration->rate_controller.maximum_gap_us,
        .rate_controller_type =
            (uint8_t)configuration->rate_controller.type,
        .acceleration_filter_type =
            (uint8_t)configuration->acceleration_filter.type,
        .behavior_id = (uint8_t)configuration->behavior.id,
        .behavior_settings_version = UINT8_C(0),
        .attitude_angle_gain_per_s = {
            configuration->roll_attitude_controller.gain_per_s,
            configuration->pitch_attitude_controller.gain_per_s,
        },
        .level_sample_duration_us =
            (uint32_t)configuration->level_calibration.sample_duration_us,
        .level_trim_degrees = {
            configuration->level_calibration.roll_trim_degrees,
            configuration->level_calibration.pitch_trim_degrees,
        },
        .acceleration_filter_cutoff_hz =
            configuration->acceleration_filter.cutoff_hz,
        .integral_activation_throttle =
            configuration->rate_controller.integral_activation_throttle,
        .easy_mode_packed =
            ((uint32_t)(configuration->easy_mode.takeoff_leveling_enabled
                            ? 1U
                            : 0U)) |
            ((uint32_t)configuration->easy_mode.armed_idle_permille << 1U) |
            ((uint32_t)configuration->easy_mode
                 .activation_throttle_permille << 11U) |
            ((uint32_t)configuration->easy_mode
                 .leveling_rate_decidegrees_per_second << 21U),
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
    for (curve = 0U; curve < RATE_CONTROLLER_AXIS_COUNT; curve++) {
        payload->rate_pid_parameters[curve][0] =
            configuration->rate_controller.axis[curve].kp;
        payload->rate_pid_parameters[curve][1] =
            configuration->rate_controller.axis[curve].ki;
        payload->rate_pid_parameters[curve][2] =
            configuration->rate_controller.axis[curve].kd;
        payload->rate_pid_parameters[curve][3] =
            configuration->rate_controller.axis[curve].integral_limit;
        payload->rate_pid_parameters[curve][4] =
            configuration->rate_controller.axis[curve].output_limit;
    }
}

bool flight_configuration_snapshot_encode(
    const flight_configuration_t *configuration,
    uint8_t *destination,
    size_t capacity,
    size_t *length)
{
    flight_configuration_payload_t payload;

    if ((configuration == NULL) || (destination == NULL) ||
        (capacity < sizeof(payload)) || (length == NULL) ||
        !flight_configuration_is_valid(configuration)) {
        return false;
    }
    encode_payload(configuration, &payload);
    memcpy(destination, &payload, sizeof(payload));
    *length = sizeof(payload);
    return true;
}

static bool decode_payload(const flight_configuration_payload_t *payload,
                           flight_configuration_t *configuration)
{
    flight_configuration_t defaults;
    control_curve_config_t *curves[4];
    size_t curve;
    size_t point;
    size_t motor;

    if ((payload->version != FLIGHT_CONFIGURATION_PAYLOAD_VERSION) ||
        (payload->schema_version != 11U) ||
        (payload->behavior_settings_version != 0U)) {
        return false;
    }
    flight_configuration_defaults(&defaults);
    *configuration = (flight_configuration_t){
        .schema_version = defaults.schema_version,
        .behavior = {
            .id = (flight_behavior_id_t)payload->behavior_id,
        },
        .propeller_layout = (propeller_layout_t)payload->propeller_layout,
        .easy_mode = {
            .takeoff_leveling_enabled =
                (payload->easy_mode_packed & UINT32_C(1)) != 0U,
            .armed_idle_permille =
                (uint16_t)((payload->easy_mode_packed >> 1U) &
                           UINT32_C(0x3ff)),
            .activation_throttle_permille =
                (uint16_t)((payload->easy_mode_packed >> 11U) &
                           UINT32_C(0x3ff)),
            .leveling_rate_decidegrees_per_second =
                (uint16_t)((payload->easy_mode_packed >> 21U) &
                           UINT32_C(0x7ff)),
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
        .level_calibration = {
            .sample_duration_us = payload->level_sample_duration_us,
            .maximum_acceleration_standard_deviation_g =
                payload->level_thresholds[0],
            .maximum_acceleration_magnitude_error_g =
                payload->level_thresholds[1],
            .maximum_trim_degrees = payload->level_thresholds[2],
            .roll_trim_degrees = payload->level_trim_degrees[0],
            .pitch_trim_degrees = payload->level_trim_degrees[1],
            .calibrated = payload->level_calibrated != 0U,
        },
        .gyro_filter = {
            .type = (flight_gyro_filter_type_t)payload->gyro_filter_type,
            .cutoff_hz = payload->processing_parameters[0],
        },
        .acceleration_filter = {
            .type = (flight_acceleration_filter_type_t)
                payload->acceleration_filter_type,
            .cutoff_hz = payload->acceleration_filter_cutoff_hz,
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
        .rate_controller = {
            .type = (rate_controller_type_t)payload->rate_controller_type,
            .maximum_gap_us = payload->rate_controller_maximum_gap_us,
            .integral_activation_throttle =
                payload->integral_activation_throttle,
        },
        .roll_attitude_controller = {
            .gain_per_s = payload->attitude_angle_gain_per_s[0],
        },
        .pitch_attitude_controller = {
            .gain_per_s = payload->attitude_angle_gain_per_s[1],
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
    for (curve = 0U; curve < RATE_CONTROLLER_AXIS_COUNT; curve++) {
        configuration->rate_controller.axis[curve] = (rate_pid_config_t){
            .kp = payload->rate_pid_parameters[curve][0],
            .ki = payload->rate_pid_parameters[curve][1],
            .kd = payload->rate_pid_parameters[curve][2],
            .integral_limit = payload->rate_pid_parameters[curve][3],
            .output_limit = payload->rate_pid_parameters[curve][4],
        };
    }
    return flight_configuration_is_valid(configuration);
}

bool flight_configuration_snapshot_decode(
    const uint8_t *source,
    size_t length,
    flight_configuration_t *configuration)
{
    flight_configuration_payload_t payload;

    if ((source == NULL) || (length != sizeof(payload)) ||
        (configuration == NULL)) {
        return false;
    }
    memcpy(&payload, source, sizeof(payload));
    return decode_payload(&payload, configuration);
}
