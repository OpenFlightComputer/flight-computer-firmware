#include "board_flight_configuration_storage.h"

#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define EXPECTED_PAYLOAD_LENGTH 412U
#define PREVIOUS_PAYLOAD_LENGTH 96U
#define OLDER_PAYLOAD_LENGTH 84U
#define OLDEST_PAYLOAD_LENGTH 88U

static board_persistent_storage_read_result_t read_result;
static board_persistent_storage_read_result_t previous_read_result;
static board_persistent_storage_read_result_t older_read_result;
static board_persistent_storage_read_result_t oldest_read_result;
static board_persistent_storage_read_result_t legacy_read_result;
static board_persistent_storage_write_result_t write_result;
static board_persistent_storage_clear_result_t clear_result;
static uint8_t payload[EXPECTED_PAYLOAD_LENGTH];
static uint8_t previous_payload[PREVIOUS_PAYLOAD_LENGTH];
static uint8_t older_payload[OLDER_PAYLOAD_LENGTH];
static uint8_t oldest_payload[OLDEST_PAYLOAD_LENGTH];
static uint8_t legacy_payload[8U];
static size_t read_length;
static size_t legacy_read_length;
static size_t write_length;
static uint32_t clear_count;

board_persistent_storage_read_result_t board_persistent_storage_read(
    void *destination, size_t length)
{
    read_length = length;
    if (length == EXPECTED_PAYLOAD_LENGTH) {
        if (read_result == BOARD_PERSISTENT_STORAGE_READ_OK) {
            memcpy(destination, payload, length);
        }
        return read_result;
    }
    if ((length == sizeof(previous_payload)) &&
        (previous_read_result == BOARD_PERSISTENT_STORAGE_READ_OK)) {
        memcpy(destination, previous_payload, length);
        return previous_read_result;
    }
    if ((length == sizeof(older_payload)) &&
        (older_read_result == BOARD_PERSISTENT_STORAGE_READ_OK)) {
        memcpy(destination, older_payload, length);
        return older_read_result;
    }
    if ((length == sizeof(oldest_payload)) &&
        (oldest_read_result == BOARD_PERSISTENT_STORAGE_READ_OK)) {
        memcpy(destination, oldest_payload, length);
        return oldest_read_result;
    }
    if (length == sizeof(previous_payload)) {
        return previous_read_result;
    }
    return length == sizeof(older_payload) ? older_read_result
                                           : oldest_read_result;
}

board_persistent_storage_read_result_t board_persistent_storage_read_legacy(
    void *destination, size_t length)
{
    legacy_read_length = length;
    if (legacy_read_result == BOARD_PERSISTENT_STORAGE_READ_OK) {
        memcpy(destination, legacy_payload, length);
    }
    return legacy_read_result;
}

board_persistent_storage_write_result_t board_persistent_storage_write(
    const void *source, size_t length)
{
    write_length = length;
    if (length <= sizeof(payload)) {
        memcpy(payload, source, length);
    }
    return write_result;
}

board_persistent_storage_clear_result_t board_persistent_storage_clear(void)
{
    clear_count++;
    return clear_result;
}

int main(void)
{
    const flight_configuration_storage_t storage =
        board_flight_configuration_storage();
    flight_configuration_t original;
    flight_configuration_t loaded;

    read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    previous_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    older_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    oldest_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    legacy_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    write_result = BOARD_PERSISTENT_STORAGE_WRITE_OK;
    clear_result = BOARD_PERSISTENT_STORAGE_CLEAR_OK;
    assert(storage.load(storage.context, &loaded) ==
           FLIGHT_CONFIGURATION_LOAD_EMPTY);
    assert(read_length == EXPECTED_PAYLOAD_LENGTH);

    flight_configuration_defaults(&original);
    original.propeller_layout = PROPELLER_LAYOUT_PROPS_OUT;
    original.motors.direction[2] = MOTOR_DIRECTION_REVERSED;
    original.mixer.yaw_factor = 0.2F;
    original.gyro_calibration.maximum_rate_dps = 4.0F;
    original.gyro_filter.cutoff_hz = 90.0F;
    assert(storage.save(storage.context, &original) ==
           FLIGHT_CONFIGURATION_SAVE_OK);
    assert(write_length == EXPECTED_PAYLOAD_LENGTH);

    read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
    assert(storage.load(storage.context, &loaded) ==
           FLIGHT_CONFIGURATION_LOAD_OK);
    assert(loaded.propeller_layout == PROPELLER_LAYOUT_PROPS_OUT);
    assert(loaded.motors.direction[2] == MOTOR_DIRECTION_REVERSED);
    assert(loaded.mixer.yaw_factor == 0.2F);
    assert(loaded.receiver_failsafe.stage_one_throttle == 0.05F);
    assert(loaded.gyro_calibration.maximum_rate_dps == 4.0F);
    assert(loaded.gyro_filter.cutoff_hz == 90.0F);
    assert(loaded.attitude_estimator.maximum_gap_us == 10000U);
    assert(loaded.control.roll.curve.point_count == 3U);
    assert(loaded.control.roll.curve.points[1].output == 0.35F);
    assert(loaded.control.throttle.curve.point_count == 2U);

    {
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
            uint8_t directions[4];
            uint8_t gyro_filter_type;
            uint8_t attitude_estimator_type;
            uint8_t reserved[1];
        } previous_payload_t;
        previous_payload_t previous = {
            .version = 3U,
            .schema_version = 3U,
            .timing_us = {25000U, 100000U, 400000U, 1500000U, 500000U},
            .mixer_factors = {0.25F, 0.25F, 0.2F},
            .failsafe_controls = {0.0F, 0.0F, 0.0F, 0.05F, 0.05F},
            .gyro_timing_us = {100000U, 600000U},
            .gyro_thresholds_dps = {4.0F, 0.4F},
            .processing_parameters = {90.0F, 0.6F},
            .attitude_maximum_gap_us = 9000U,
            .propeller_layout = (uint8_t)PROPELLER_LAYOUT_PROPS_OUT,
            .directions = {0U, 1U, 0U, 1U},
        };

        assert(sizeof(previous) == sizeof(previous_payload));
        memcpy(previous_payload, &previous, sizeof(previous));
        read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        previous_read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
        assert(storage.load(storage.context, &loaded) ==
               FLIGHT_CONFIGURATION_LOAD_OK);
        assert(loaded.schema_version == 4U);
        assert(loaded.gyro_filter.cutoff_hz == 90.0F);
        assert(loaded.control.throttle.maximum == 1.0F);
    }

    {
        typedef struct {
            uint32_t version;
            uint32_t schema_version;
            uint32_t timing_us[5];
            float mixer_factors[3];
            float failsafe_controls[5];
            uint32_t gyro_timing_us[2];
            float gyro_thresholds_dps[2];
            uint8_t propeller_layout;
            uint8_t directions[4];
            uint8_t reserved[3];
        } older_payload_t;
        older_payload_t older = {
            .version = 2U,
            .schema_version = 2U,
            .timing_us = {25000U, 100000U, 400000U, 1500000U, 500000U},
            .mixer_factors = {0.25F, 0.25F, 0.2F},
            .failsafe_controls = {0.0F, 0.0F, 0.0F, 0.05F, 0.05F},
            .gyro_timing_us = {100000U, 600000U},
            .gyro_thresholds_dps = {4.0F, 0.4F},
            .propeller_layout = (uint8_t)PROPELLER_LAYOUT_PROPS_OUT,
            .directions = {0U, 1U, 0U, 1U},
        };

        assert(sizeof(older) == sizeof(older_payload));
        memcpy(older_payload, &older, sizeof(older));
        read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        previous_read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        older_read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
        assert(storage.load(storage.context, &loaded) ==
               FLIGHT_CONFIGURATION_LOAD_OK);
        assert(loaded.schema_version == 4U);
        assert(loaded.propeller_layout == PROPELLER_LAYOUT_PROPS_OUT);
        assert(loaded.motors.direction[1] == MOTOR_DIRECTION_REVERSED);
        assert(loaded.mixer.yaw_factor == 0.2F);
        assert(loaded.gyro_calibration.sample_duration_us == 600000U);
        assert(loaded.gyro_filter.cutoff_hz == 80.0F);
    }

    {
        typedef struct {
            uint32_t version;
            uint32_t schema_version;
            uint64_t timing_us[5];
            float mixer_factors[3];
            float failsafe_controls[5];
            uint8_t propeller_layout;
            uint8_t directions[4];
            uint8_t reserved[3];
        } oldest_payload_t;
        oldest_payload_t oldest = {
            .version = 1U,
            .schema_version = 1U,
            .timing_us = {25000U, 100000U, 400000U, 1500000U, 500000U},
            .mixer_factors = {0.25F, 0.25F, 0.2F},
            .failsafe_controls = {0.0F, 0.0F, 0.0F, 0.05F, 0.05F},
            .propeller_layout = (uint8_t)PROPELLER_LAYOUT_PROPS_OUT,
            .directions = {0U, 1U, 0U, 1U},
        };

        assert(sizeof(oldest) == sizeof(oldest_payload));
        memcpy(oldest_payload, &oldest, sizeof(oldest));
        read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        previous_read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        older_read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
        oldest_read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
        assert(storage.load(storage.context, &loaded) ==
               FLIGHT_CONFIGURATION_LOAD_OK);
        assert(loaded.schema_version == 4U);
        assert(loaded.gyro_filter.cutoff_hz == 80.0F);
    }

    read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
    previous_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    older_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    oldest_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    legacy_read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
    memset(legacy_payload, 0, sizeof(legacy_payload));
    legacy_payload[0] = 1U;
    legacy_payload[4] = (uint8_t)MOTOR_DIRECTION_NORMAL;
    legacy_payload[5] = (uint8_t)MOTOR_DIRECTION_REVERSED;
    legacy_payload[6] = (uint8_t)MOTOR_DIRECTION_NORMAL;
    legacy_payload[7] = (uint8_t)MOTOR_DIRECTION_REVERSED;
    assert(storage.load(storage.context, &loaded) ==
           FLIGHT_CONFIGURATION_LOAD_OK);
    assert(legacy_read_length == sizeof(legacy_payload));
    assert(loaded.propeller_layout == PROPELLER_LAYOUT_PROPS_IN);
    assert(loaded.motors.direction[1] == MOTOR_DIRECTION_REVERSED);
    assert(loaded.motors.direction[3] == MOTOR_DIRECTION_REVERSED);
    assert(loaded.mixer.roll_factor == 0.25F);

    legacy_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
    payload[0] = 5U;
    assert(storage.load(storage.context, &loaded) ==
           FLIGHT_CONFIGURATION_LOAD_ERROR);
    assert(storage.clear(storage.context) == FLIGHT_CONFIGURATION_CLEAR_OK);
    assert(clear_count == 1U);
    clear_result = BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    assert(storage.clear(storage.context) ==
           FLIGHT_CONFIGURATION_CLEAR_ERROR);
    return 0;
}
