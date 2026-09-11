#ifndef OPENFLIGHTCOMPUTER_YAW_ATTITUDE_CONTROLLER_H
#define OPENFLIGHTCOMPUTER_YAW_ATTITUDE_CONTROLLER_H

#include <stdbool.h>

bool yaw_attitude_controller_update(float requested_rate_dps,
                                    float maximum_rate_dps,
                                    float *desired_rate_dps);

#endif
