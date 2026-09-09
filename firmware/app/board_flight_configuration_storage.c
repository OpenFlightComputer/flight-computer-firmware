#include "board_flight_configuration_storage.h"

#include "board.h"

#include <stdint.h>

#define FLIGHT_CONFIGURATION_PAYLOAD_VERSION UINT32_C(1)
#define LEGACY_MOTOR_CONFIGURATION_PAYLOAD_VERSION UINT32_C(1)

typedef struct {
    uint32_t version;
    uint32_t schema_version;
    uint64_t timing_us[5];
    float mixer_factors[3];
    float failsafe_controls[5];
    uint8_t propeller_layout;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
    uint8_t reserved[3];
} flight_configuration_payload_t;

typedef struct {
    uint32_t version;
    uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT];
} legacy_motor_configuration_payload_t;

_Static_assert(sizeof(flight_configuration_payload_t) == 88U,
               "Flight configuration payload format changed");

static void encode(const flight_configuration_t *configuration,
                   flight_configuration_payload_t *payload)
{
    size_t motor;

    *payload = (flight_configuration_payload_t){
        .version = FLIGHT_CONFIGURATION_PAYLOAD_VERSION,
        .schema_version = configuration->schema_version,
        .timing_us = {
            configuration->receiver_failsafe.stale_after_us,
            configuration->receiver_failsafe.loss_detected_after_us,
            configuration->receiver_failsafe.hold_last_until_us,
            configuration->receiver_failsafe.stage_two_after_us,
            configuration->receiver_failsafe.recovery_stable_us,
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
    };
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        payload->directions[motor] =
            (uint8_t)configuration->motors.direction[motor];
    }
}

static bool decode(const flight_configuration_payload_t *payload,
                   flight_configuration_t *configuration)
{
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
    if ((result == BOARD_PERSISTENT_STORAGE_READ_OK) &&
        decode(&payload, configuration)) {
        return FLIGHT_CONFIGURATION_LOAD_OK;
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
