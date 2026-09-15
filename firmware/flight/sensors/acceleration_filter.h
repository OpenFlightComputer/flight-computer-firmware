#ifndef OPENFLIGHTCOMPUTER_ACCELERATION_FILTER_H
#define OPENFLIGHTCOMPUTER_ACCELERATION_FILTER_H

#include <stdbool.h>

typedef enum {
    ACCELERATION_FILTER_FIRST_ORDER_LOW_PASS = 0,
    ACCELERATION_FILTER_TYPE_COUNT,
} acceleration_filter_type_t;

typedef struct {
    acceleration_filter_type_t type;
    float cutoff_hz;
} acceleration_filter_config_t;

typedef struct {
    acceleration_filter_config_t config;
    float time_constant_seconds;
    float output_g[3];
    bool sample_seen;
    bool initialized;
} acceleration_filter_t;

bool acceleration_filter_config_is_valid(
    const acceleration_filter_config_t *config);
bool acceleration_filter_initialize(
    acceleration_filter_t *filter,
    const acceleration_filter_config_t *config);
void acceleration_filter_reset(acceleration_filter_t *filter);
bool acceleration_filter_process(acceleration_filter_t *filter,
                                 const float input_g[3],
                                 float dt_seconds,
                                 float output_g[3]);
const char *acceleration_filter_type_name(acceleration_filter_type_t type);

#endif
