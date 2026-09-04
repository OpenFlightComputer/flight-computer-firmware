#include "dshot_encoder.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint16_t reference_encode(uint16_t value, bool telemetry)
{
    const uint16_t payload =
        (uint16_t)((value << 1U) | (telemetry ? 1U : 0U));
    uint16_t checksum_data = payload;
    uint16_t checksum = 0U;
    unsigned int nibble;

    for (nibble = 0U; nibble < 3U; nibble++) {
        checksum ^= checksum_data;
        checksum_data >>= 4U;
    }
    return (uint16_t)((payload << 4U) | (checksum & 0x0FU));
}

static void known_frames_match_the_protocol(void)
{
    uint16_t frame = UINT16_MAX;

    assert(dshot_encode_throttle(DSHOT_VALUE_STOP, false, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0x0000));
    assert(dshot_encode_throttle(DSHOT_VALUE_STOP, true, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0x0011));
    assert(dshot_encode_throttle(UINT16_C(1046), false, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0x82C6));
    assert(dshot_encode_throttle(DSHOT_THROTTLE_MAX, false, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0xFFEE));
    assert(dshot_encode_throttle(DSHOT_THROTTLE_MAX, true, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0xFFFF));
    assert(dshot_encode_command(DSHOT_COMMAND_MIN, false, &frame) ==
           DSHOT_ENCODE_OK);
    assert(frame == UINT16_C(0x0022));
}

static void all_values_and_telemetry_states_are_encoded(void)
{
    uint16_t value;
    unsigned int telemetry;

    for (telemetry = 0U; telemetry <= 1U; telemetry++) {
        uint16_t frame;

        assert(dshot_encode_throttle(DSHOT_VALUE_STOP,
                                     telemetry != 0U,
                                     &frame) == DSHOT_ENCODE_OK);
        assert(frame == reference_encode(DSHOT_VALUE_STOP,
                                         telemetry != 0U));

        for (value = DSHOT_COMMAND_MIN;
             value <= DSHOT_COMMAND_MAX;
             value++) {
            assert(dshot_encode_command(value, telemetry != 0U, &frame) ==
                   DSHOT_ENCODE_OK);
            assert(frame == reference_encode(value, telemetry != 0U));
        }
        for (value = DSHOT_THROTTLE_MIN;
             value <= DSHOT_THROTTLE_MAX;
             value++) {
            assert(dshot_encode_throttle(value, telemetry != 0U, &frame) ==
                   DSHOT_ENCODE_OK);
            assert(frame == reference_encode(value, telemetry != 0U));
            assert((frame >> 5U) == value);
            assert(((frame >> 4U) & 1U) == telemetry);
        }
    }
}

static void categories_cannot_be_crossed(void)
{
    uint16_t value;

    for (value = DSHOT_COMMAND_MIN; value <= DSHOT_COMMAND_MAX; value++) {
        uint16_t frame = UINT16_C(0xA55A);

        assert(dshot_encode_throttle(value, false, &frame) ==
               DSHOT_ENCODE_INVALID_THROTTLE);
        assert(frame == UINT16_C(0xA55A));
    }

    value = UINT16_C(0xA55A);
    assert(dshot_encode_command(DSHOT_VALUE_STOP, false, &value) ==
           DSHOT_ENCODE_INVALID_COMMAND);
    assert(value == UINT16_C(0xA55A));
    assert(dshot_encode_command(DSHOT_THROTTLE_MIN, false, &value) ==
           DSHOT_ENCODE_INVALID_COMMAND);
    assert(value == UINT16_C(0xA55A));
    assert(dshot_encode_throttle(DSHOT_THROTTLE_MAX + 1U, false, &value) ==
           DSHOT_ENCODE_INVALID_THROTTLE);
    assert(value == UINT16_C(0xA55A));
}

static void null_destinations_are_rejected(void)
{
    assert(dshot_encode_throttle(DSHOT_VALUE_STOP, false, NULL) ==
           DSHOT_ENCODE_INVALID_ARGUMENT);
    assert(dshot_encode_command(DSHOT_COMMAND_MIN, false, NULL) ==
           DSHOT_ENCODE_INVALID_ARGUMENT);
}

int main(void)
{
    known_frames_match_the_protocol();
    all_values_and_telemetry_states_are_encoded();
    categories_cannot_be_crossed();
    null_destinations_are_rejected();
    return 0;
}
