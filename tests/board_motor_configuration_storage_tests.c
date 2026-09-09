#include "board_motor_configuration_storage.h"

#include "board.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define EXPECTED_PAYLOAD_LENGTH 8U

static board_persistent_storage_read_result_t fake_read_result;
static board_persistent_storage_write_result_t fake_write_result;
static board_persistent_storage_clear_result_t fake_clear_result;
static uint8_t fake_read_payload[EXPECTED_PAYLOAD_LENGTH];
static uint8_t fake_written_payload[EXPECTED_PAYLOAD_LENGTH];
static size_t fake_read_length;
static size_t fake_write_length;
static uint32_t fake_clear_count;

board_persistent_storage_read_result_t board_persistent_storage_read(
    void *destination,
    size_t length)
{
    fake_read_length = length;
    if (fake_read_result == BOARD_PERSISTENT_STORAGE_READ_OK) {
        memcpy(destination, fake_read_payload, length);
    }
    return fake_read_result;
}

board_persistent_storage_write_result_t board_persistent_storage_write(
    const void *source,
    size_t length)
{
    fake_write_length = length;
    if (length <= sizeof(fake_written_payload)) {
        memcpy(fake_written_payload, source, length);
    }
    return fake_write_result;
}

board_persistent_storage_clear_result_t board_persistent_storage_clear(void)
{
    fake_clear_count++;
    return fake_clear_result;
}

static void reset_fakes(void)
{
    fake_read_result = BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    fake_write_result = BOARD_PERSISTENT_STORAGE_WRITE_OK;
    fake_clear_result = BOARD_PERSISTENT_STORAGE_CLEAR_OK;
    memset(fake_read_payload, 0, sizeof(fake_read_payload));
    memset(fake_written_payload, 0, sizeof(fake_written_payload));
    fake_read_length = 0U;
    fake_write_length = 0U;
    fake_clear_count = 0U;
}

static void put_payload(uint32_t version,
                        uint8_t motor_1,
                        uint8_t motor_2,
                        uint8_t motor_3,
                        uint8_t motor_4)
{
    const uint8_t directions[MOTOR_COMMAND_MOTOR_COUNT] = {
        motor_1,
        motor_2,
        motor_3,
        motor_4,
    };

    memcpy(fake_read_payload, &version, sizeof(version));
    memcpy(&fake_read_payload[sizeof(version)],
           directions,
           sizeof(directions));
}

static void load_maps_empty_valid_and_corrupt_storage(void)
{
    const motor_configuration_storage_t storage =
        board_motor_configuration_storage();
    motor_configuration_t configuration;

    reset_fakes();
    assert(storage.load(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_LOAD_EMPTY);
    assert(fake_read_length == EXPECTED_PAYLOAD_LENGTH);

    fake_read_result = BOARD_PERSISTENT_STORAGE_READ_OK;
    put_payload(1U, 0U, 1U, 0U, 1U);
    assert(storage.load(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_LOAD_OK);
    assert(configuration.direction[0] == MOTOR_DIRECTION_NORMAL);
    assert(configuration.direction[1] == MOTOR_DIRECTION_REVERSED);
    assert(configuration.direction[2] == MOTOR_DIRECTION_NORMAL);
    assert(configuration.direction[3] == MOTOR_DIRECTION_REVERSED);

    put_payload(2U, 0U, 1U, 0U, 1U);
    assert(storage.load(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_LOAD_ERROR);
    put_payload(1U, 0U, 1U, (uint8_t)MOTOR_DIRECTION_COUNT, 1U);
    assert(storage.load(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_LOAD_ERROR);
    fake_read_result = BOARD_PERSISTENT_STORAGE_READ_ERROR;
    assert(storage.load(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_LOAD_ERROR);
    assert(storage.load(storage.context, NULL) ==
           MOTOR_CONFIGURATION_LOAD_ERROR);
}

static void save_serializes_valid_configuration_and_maps_errors(void)
{
    const motor_configuration_storage_t storage =
        board_motor_configuration_storage();
    motor_configuration_t configuration;
    uint32_t stored_version = 0U;

    reset_fakes();
    motor_configuration_defaults(&configuration);
    configuration.direction[2] = MOTOR_DIRECTION_REVERSED;
    assert(storage.save(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_SAVE_OK);
    assert(fake_write_length == EXPECTED_PAYLOAD_LENGTH);
    memcpy(&stored_version, fake_written_payload, sizeof(stored_version));
    assert(stored_version == 1U);
    assert(fake_written_payload[4] == (uint8_t)MOTOR_DIRECTION_NORMAL);
    assert(fake_written_payload[5] == (uint8_t)MOTOR_DIRECTION_NORMAL);
    assert(fake_written_payload[6] == (uint8_t)MOTOR_DIRECTION_REVERSED);
    assert(fake_written_payload[7] == (uint8_t)MOTOR_DIRECTION_NORMAL);

    fake_write_result = BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    assert(storage.save(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_SAVE_ERROR);
    configuration.direction[0] = MOTOR_DIRECTION_COUNT;
    fake_write_length = 0U;
    assert(storage.save(storage.context, &configuration) ==
           MOTOR_CONFIGURATION_SAVE_ERROR);
    assert(fake_write_length == 0U);
    assert(storage.save(storage.context, NULL) ==
           MOTOR_CONFIGURATION_SAVE_ERROR);
}

static void clear_maps_board_result(void)
{
    const motor_configuration_storage_t storage =
        board_motor_configuration_storage();

    reset_fakes();
    assert(storage.clear(storage.context) == MOTOR_CONFIGURATION_CLEAR_OK);
    assert(fake_clear_count == 1U);
    fake_clear_result = BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    assert(storage.clear(storage.context) == MOTOR_CONFIGURATION_CLEAR_ERROR);
    assert(fake_clear_count == 2U);
}

int main(void)
{
    load_maps_empty_valid_and_corrupt_storage();
    save_serializes_valid_configuration_and_maps_errors();
    clear_maps_board_result();
    return 0;
}
