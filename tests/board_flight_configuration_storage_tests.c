#include "board_flight_configuration_storage.h"

#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define EXPECTED_PAYLOAD_LENGTH 88U

static board_persistent_storage_read_result_t read_result;
static board_persistent_storage_read_result_t legacy_read_result;
static board_persistent_storage_write_result_t write_result;
static board_persistent_storage_clear_result_t clear_result;
static uint8_t payload[EXPECTED_PAYLOAD_LENGTH];
static uint8_t legacy_payload[8U];
static size_t read_length;
static size_t legacy_read_length;
static size_t write_length;
static uint32_t clear_count;

board_persistent_storage_read_result_t board_persistent_storage_read(
    void *destination, size_t length)
{
    read_length = length;
    if (read_result == BOARD_PERSISTENT_STORAGE_READ_OK) {
        memcpy(destination, payload, length);
    }
    return read_result;
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

    read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
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
    payload[0] = 2U;
    assert(storage.load(storage.context, &loaded) ==
           FLIGHT_CONFIGURATION_LOAD_ERROR);
    assert(storage.clear(storage.context) == FLIGHT_CONFIGURATION_CLEAR_OK);
    assert(clear_count == 1U);
    clear_result = BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    assert(storage.clear(storage.context) ==
           FLIGHT_CONFIGURATION_CLEAR_ERROR);
    return 0;
}
