#include "dshot_encoder.h"

#include <stddef.h>

static uint16_t convert_value_to_payload(uint16_t value)
{
    /* Move the 11-bit DShot value left to make room for telemetry. */
    return (uint16_t)(value << 1U);
}

static uint16_t add_telemetry_request(uint16_t payload,
                                      bool request_telemetry)
{
    /* The least-significant payload bit asks the ESC for telemetry. */
    return (uint16_t)(payload | (request_telemetry ? 1U : 0U));
}

static uint16_t calculate_checksum(uint16_t payload)
{
    /* XOR the three four-bit payload nibbles into one checksum nibble. */
    return (uint16_t)((payload ^ (payload >> 4U) ^ (payload >> 8U)) &
                      0x0FU);
}

static uint16_t encode_value(uint16_t value, bool request_telemetry)
{
    uint16_t payload = convert_value_to_payload(value);
    payload = add_telemetry_request(payload, request_telemetry);

    const uint16_t checksum = calculate_checksum(payload);

    return (uint16_t)((payload << 4U) | checksum);
}

dshot_encode_result_t dshot_encode_throttle(uint16_t throttle,
                                            bool request_telemetry,
                                            uint16_t *frame)
{
    if (frame == NULL) {
        return DSHOT_ENCODE_INVALID_ARGUMENT;
    }
    if ((throttle != DSHOT_VALUE_STOP) &&
        ((throttle < DSHOT_THROTTLE_MIN) ||
         (throttle > DSHOT_THROTTLE_MAX))) {
        return DSHOT_ENCODE_INVALID_THROTTLE;
    }

    *frame = encode_value(throttle, request_telemetry);
    return DSHOT_ENCODE_OK;
}

dshot_encode_result_t dshot_encode_command(uint16_t command,
                                           bool request_telemetry,
                                           uint16_t *frame)
{
    if (frame == NULL) {
        return DSHOT_ENCODE_INVALID_ARGUMENT;
    }
    if ((command < DSHOT_COMMAND_MIN) || (command > DSHOT_COMMAND_MAX)) {
        return DSHOT_ENCODE_INVALID_COMMAND;
    }

    *frame = encode_value(command, request_telemetry);
    return DSHOT_ENCODE_OK;
}
