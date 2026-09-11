#ifndef OPENFLIGHTCOMPUTER_QUAD_X_MIXER_H
#define OPENFLIGHTCOMPUTER_QUAD_X_MIXER_H

#include "motor_command.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PROPELLER_LAYOUT_PROPS_IN = 0,
    PROPELLER_LAYOUT_PROPS_OUT,
    PROPELLER_LAYOUT_COUNT,
} propeller_layout_t;

typedef struct {
    float coefficient[MOTOR_COMMAND_MOTOR_COUNT][3];
    bool initialized;
} prepared_quad_x_mixer_t;

typedef struct {
    motor_command_t command;
    float correction_scale;
    float collective_shift;
    bool saturated;
} quad_x_mixer_output_t;

bool quad_x_mixer_prepare(propeller_layout_t layout,
                          prepared_quad_x_mixer_t *prepared);
bool quad_x_mixer_apply_prepared(const prepared_quad_x_mixer_t *prepared,
                                 float throttle,
                                 const float correction[3],
                                 uint64_t timestamp_us,
                                 quad_x_mixer_output_t *output);
bool quad_x_mixer_apply(propeller_layout_t layout,
                        float throttle,
                        const float correction[3],
                        uint64_t timestamp_us,
                        quad_x_mixer_output_t *output);
const char *propeller_layout_name(propeller_layout_t layout);

#endif
