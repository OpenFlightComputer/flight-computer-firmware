#ifndef OPENFLIGHTCOMPUTER_ATTITUDE_ESTIMATOR_H
#define OPENFLIGHTCOMPUTER_ATTITUDE_ESTIMATOR_H

#include "accelerometer_attitude.h"

#include <stdbool.h>

typedef enum {
    ATTITUDE_ESTIMATOR_COMPLEMENTARY = 0,
    ATTITUDE_ESTIMATOR_TYPE_COUNT,
} attitude_estimator_type_t;

typedef struct {
    attitude_estimator_type_t type;
    float accelerometer_correction_time_constant_s;
} attitude_estimator_config_t;

typedef struct {
    attitude_estimator_config_t config;
    float roll_degrees;
    float pitch_degrees;
    bool estimate_seen;
    bool initialized;
} attitude_estimator_t;

bool attitude_estimator_config_is_valid(
    const attitude_estimator_config_t *config);
bool attitude_estimator_initialize(
    attitude_estimator_t *estimator,
    const attitude_estimator_config_t *config);
void attitude_estimator_reset(attitude_estimator_t *estimator);
bool attitude_estimator_process(
    attitude_estimator_t *estimator,
    const accelerometer_attitude_t *accelerometer,
    const float filtered_gyroscope_dps[3],
    float dt_seconds,
    float *roll_degrees,
    float *pitch_degrees);
const char *attitude_estimator_type_name(attitude_estimator_type_t type);

#endif
