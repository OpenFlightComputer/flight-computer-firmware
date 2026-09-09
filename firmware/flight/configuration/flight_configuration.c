#include "flight_configuration.h"

#include "flight_configuration_defaults.h"

#include <stddef.h>

void flight_configuration_defaults(flight_configuration_t *configuration)
{
    static const motor_direction_t directions[MOTOR_COMMAND_MOTOR_COUNT] =
        OFC_DEFAULT_MOTOR_DIRECTIONS;
    size_t motor;

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
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->motors.direction[motor] = directions[motor];
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
           receiver_failsafe_config_is_valid(
               &configuration->receiver_failsafe);
}
