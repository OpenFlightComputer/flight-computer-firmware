#include "board_motor_configuration_storage.h"

#include "board.h"

#include <stdint.h>

#define MOTOR_CONFIGURATION_PAYLOAD_VERSION UINT32_C(1)

typedef struct {
    uint32_t version;
    uint8_t direction[MOTOR_COMMAND_MOTOR_COUNT];
} motor_configuration_payload_t;

_Static_assert(sizeof(motor_configuration_payload_t) == 8U,
               "Motor configuration payload format changed");

static motor_configuration_load_result_t load_configuration(
    void *context,
    motor_configuration_t *configuration)
{
    motor_configuration_payload_t payload;
    board_persistent_storage_read_result_t result;
    size_t motor;

    (void)context;
    if (configuration == NULL) {
        return MOTOR_CONFIGURATION_LOAD_ERROR;
    }

    result = board_persistent_storage_read(&payload, sizeof(payload));
    if (result == BOARD_PERSISTENT_STORAGE_READ_EMPTY) {
        return MOTOR_CONFIGURATION_LOAD_EMPTY;
    }
    if ((result != BOARD_PERSISTENT_STORAGE_READ_OK) ||
        (payload.version != MOTOR_CONFIGURATION_PAYLOAD_VERSION)) {
        return MOTOR_CONFIGURATION_LOAD_ERROR;
    }

    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        configuration->direction[motor] =
            (motor_direction_t)payload.direction[motor];
    }
    return motor_configuration_is_valid(configuration)
               ? MOTOR_CONFIGURATION_LOAD_OK
               : MOTOR_CONFIGURATION_LOAD_ERROR;
}

static motor_configuration_save_result_t save_configuration(
    void *context,
    const motor_configuration_t *configuration)
{
    motor_configuration_payload_t payload = {
        .version = MOTOR_CONFIGURATION_PAYLOAD_VERSION,
    };
    size_t motor;

    (void)context;
    if (!motor_configuration_is_valid(configuration)) {
        return MOTOR_CONFIGURATION_SAVE_ERROR;
    }
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        payload.direction[motor] =
            (uint8_t)configuration->direction[motor];
    }

    return board_persistent_storage_write(&payload, sizeof(payload)) ==
                   BOARD_PERSISTENT_STORAGE_WRITE_OK
               ? MOTOR_CONFIGURATION_SAVE_OK
               : MOTOR_CONFIGURATION_SAVE_ERROR;
}

static motor_configuration_clear_result_t clear_configuration(void *context)
{
    (void)context;
    return board_persistent_storage_clear() ==
                   BOARD_PERSISTENT_STORAGE_CLEAR_OK
               ? MOTOR_CONFIGURATION_CLEAR_OK
               : MOTOR_CONFIGURATION_CLEAR_ERROR;
}

motor_configuration_storage_t board_motor_configuration_storage(void)
{
    return (motor_configuration_storage_t){
        .load = load_configuration,
        .save = save_configuration,
        .clear = clear_configuration,
    };
}
