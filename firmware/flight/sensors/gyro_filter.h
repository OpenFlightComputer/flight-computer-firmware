#ifndef OPENFLIGHTCOMPUTER_GYRO_FILTER_H
#define OPENFLIGHTCOMPUTER_GYRO_FILTER_H

#include <stdbool.h>

typedef enum {
    GYRO_FILTER_FIRST_ORDER_LOW_PASS = 0,
    GYRO_FILTER_TYPE_COUNT,
} gyro_filter_type_t;

typedef struct {
    gyro_filter_type_t type;
    float cutoff_hz;
} gyro_filter_config_t;

typedef struct {
    gyro_filter_config_t config;
    float output_dps[3];
    bool sample_seen;
    bool initialized;
} gyro_filter_t;

bool gyro_filter_config_is_valid(const gyro_filter_config_t *config);
bool gyro_filter_initialize(gyro_filter_t *filter,
                            const gyro_filter_config_t *config);
void gyro_filter_reset(gyro_filter_t *filter);
bool gyro_filter_process(gyro_filter_t *filter,
                         const float input_dps[3],
                         float dt_seconds,
                         float output_dps[3]);
const char *gyro_filter_type_name(gyro_filter_type_t type);

#endif
