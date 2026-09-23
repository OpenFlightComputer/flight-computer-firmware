#include "flight_runtime_configuration.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001F;
}

int main(void)
{
    flight_configuration_t configuration;
    flight_runtime_configuration_t runtime;

    flight_configuration_defaults(&configuration);
    assert(flight_runtime_configuration_prepare(
        &configuration, 4096.0F, 16.384F, &runtime));
    assert(runtime.initialized);
    assert(runtime.control_input.initialized);
    assert(runtime.mixer.initialized);
    assert(runtime.control_profile.initialized);
    assert(runtime.rate_controller.initialized);
    assert(close_enough(runtime.imu_processing.acceleration_counts_per_g,
                        4096.0F));
    assert(close_enough(runtime.imu_processing.gyroscope_counts_per_dps,
                        16.384F));
    assert(runtime.receiver_freshness.fresh_through_us ==
           configuration.receiver_failsafe.stale_after_us);
    assert(runtime.receiver_freshness.lost_after_us ==
           configuration.receiver_failsafe.loss_detected_after_us);
    assert(runtime.level_calibrated ==
           configuration.level_calibration.calibrated);
    assert(!flight_runtime_configuration_prepare(
        NULL, 4096.0F, 16.384F, &runtime));
    assert(!flight_runtime_configuration_prepare(
        &configuration, 0.0F, 16.384F, &runtime));
    assert(!flight_runtime_configuration_prepare(
        &configuration, 4096.0F, 0.0F, &runtime));
    assert(!flight_runtime_configuration_prepare(
        &configuration, 4096.0F, 16.384F, NULL));

    puts("flight runtime configuration tests passed");
    return 0;
}
