#ifndef OPENFLIGHTCOMPUTER_QUAD_X_MIXER_H
#define OPENFLIGHTCOMPUTER_QUAD_X_MIXER_H

#include "motor_command.h"
#include "receiver_normalization.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PROPELLER_LAYOUT_PROPS_IN = 0,
    PROPELLER_LAYOUT_PROPS_OUT,
    PROPELLER_LAYOUT_COUNT,
} propeller_layout_t;

typedef struct {
    float roll_factor;
    float pitch_factor;
    float yaw_factor;
} quad_x_mixer_config_t;

typedef struct {
    float coefficient[MOTOR_COMMAND_MOTOR_COUNT][3];
    bool initialized;
} prepared_quad_x_mixer_t;

bool quad_x_mixer_config_is_valid(const quad_x_mixer_config_t *config);
bool quad_x_mixer_prepare(const quad_x_mixer_config_t *config,
                          propeller_layout_t layout,
                          prepared_quad_x_mixer_t *prepared);
bool quad_x_mixer_apply_prepared(const prepared_quad_x_mixer_t *prepared,
                                 const receiver_control_snapshot_t *control,
                                 uint64_t timestamp_us,
                                 motor_command_t *command);
bool quad_x_mixer_apply(const quad_x_mixer_config_t *config,
                        propeller_layout_t layout,
                        const receiver_control_snapshot_t *control,
                        uint64_t timestamp_us,
                        motor_command_t *command);
const char *propeller_layout_name(propeller_layout_t layout);

#endif
