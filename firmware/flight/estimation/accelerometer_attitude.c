#include "accelerometer_attitude.h"

#include <math.h>
#include <stddef.h>

#define RADIANS_TO_DEGREES 57.2957795130823208768F
#define MINIMUM_ACCELERATION_SQUARED 0.000001F

bool accelerometer_attitude_calculate(
    const float acceleration_g[3],
    accelerometer_attitude_t *attitude)
{
    float horizontal_squared;
    float magnitude_squared;

    if ((acceleration_g == NULL) || (attitude == NULL) ||
        !isfinite(acceleration_g[0]) || !isfinite(acceleration_g[1]) ||
        !isfinite(acceleration_g[2])) {
        return false;
    }
    horizontal_squared =
        (acceleration_g[1] * acceleration_g[1]) +
        (acceleration_g[2] * acceleration_g[2]);
    magnitude_squared = horizontal_squared +
                        (acceleration_g[0] * acceleration_g[0]);
    if (!isfinite(magnitude_squared) ||
        (magnitude_squared < MINIMUM_ACCELERATION_SQUARED)) {
        return false;
    }

    *attitude = (accelerometer_attitude_t){
        /* Body axes are forward/right/down; static specific force is up. */
        .roll_degrees =
            atan2f(-acceleration_g[1], -acceleration_g[2]) *
            RADIANS_TO_DEGREES,
        .pitch_degrees =
            atan2f(acceleration_g[0], sqrtf(horizontal_squared)) *
            RADIANS_TO_DEGREES,
    };
    return isfinite(attitude->roll_degrees) &&
           isfinite(attitude->pitch_degrees);
}
