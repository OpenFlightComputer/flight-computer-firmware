#ifndef OPENFLIGHTCOMPUTER_ACCELEROMETER_ATTITUDE_H
#define OPENFLIGHTCOMPUTER_ACCELEROMETER_ATTITUDE_H

#include <stdbool.h>

typedef struct {
    float roll_degrees;
    float pitch_degrees;
} accelerometer_attitude_t;

bool accelerometer_attitude_calculate(
    const float acceleration_g[3],
    accelerometer_attitude_t *attitude);

#endif
