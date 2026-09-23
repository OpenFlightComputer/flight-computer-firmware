#include "flight_configuration_snapshot.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t snapshot[FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY];
    flight_configuration_t original;
    flight_configuration_t decoded;
    size_t length = 0U;

    flight_configuration_defaults(&original);
    original.control.roll.deadband = 0.123F;
    original.level_calibration.calibrated = true;
    original.level_calibration.roll_trim_degrees = 1.25F;
    original.level_calibration.pitch_trim_degrees = -2.5F;
    assert(flight_configuration_snapshot_encode(
        &original, snapshot, sizeof(snapshot), &length));
    assert(length == FLIGHT_CONFIGURATION_SNAPSHOT_CAPACITY);
    assert(flight_configuration_snapshot_decode(
        snapshot, length, &decoded));
    assert(memcmp(&original, &decoded, sizeof(original)) == 0);

    snapshot[0] = UINT8_C(99);
    assert(!flight_configuration_snapshot_decode(
        snapshot, length, &decoded));
    assert(!flight_configuration_snapshot_decode(NULL, length, &decoded));
    assert(!flight_configuration_snapshot_decode(
        snapshot, length - 1U, &decoded));
    assert(!flight_configuration_snapshot_decode(
        snapshot, length, NULL));

    puts("flight configuration snapshot tests passed");
    return 0;
}
