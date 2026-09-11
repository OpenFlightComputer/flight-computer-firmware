#include "yaw_attitude_controller.h"

#include <math.h>
#include <stddef.h>

#define MAXIMUM_RATE_DPS 2000.0F

bool yaw_attitude_controller_update(float requested_rate_dps,
                                    float maximum_rate_dps,
                                    float *desired_rate_dps)
{
    if (!isfinite(requested_rate_dps) || !isfinite(maximum_rate_dps) ||
        (maximum_rate_dps <= 0.0F) ||
        (maximum_rate_dps > MAXIMUM_RATE_DPS) ||
        (desired_rate_dps == NULL)) {
        return false;
    }

    /*
     * The current vehicle has no magnetometer or other absolute-heading
     * source, so an outer yaw-angle controller cannot be implemented yet.
     * Preserve the pilot's desired yaw rate for the inner yaw-rate PID.
     */
    if (requested_rate_dps > maximum_rate_dps) {
        *desired_rate_dps = maximum_rate_dps;
    } else if (requested_rate_dps < -maximum_rate_dps) {
        *desired_rate_dps = -maximum_rate_dps;
    } else {
        *desired_rate_dps = requested_rate_dps;
    }
    return true;
}
