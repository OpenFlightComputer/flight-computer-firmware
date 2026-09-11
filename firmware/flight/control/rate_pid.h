#ifndef OPENFLIGHTCOMPUTER_RATE_PID_H
#define OPENFLIGHTCOMPUTER_RATE_PID_H

#include <stdbool.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_limit;
    float output_limit;
} rate_pid_config_t;

typedef struct {
    float proportional;
    float integral;
    float derivative;
    float total;
} rate_pid_output_t;

typedef struct {
    rate_pid_config_t config;
    float integral;
    float previous_measurement;
    bool measurement_seeded;
    bool initialized;
} rate_pid_t;

bool rate_pid_config_is_valid(const rate_pid_config_t *config);
bool rate_pid_initialize(rate_pid_t *pid, const rate_pid_config_t *config);
void rate_pid_reset(rate_pid_t *pid);
bool rate_pid_seed_measurement(rate_pid_t *pid, float measurement);
bool rate_pid_update(rate_pid_t *pid,
                     float desired_rate,
                     float measured_rate,
                     float dt_seconds,
                     rate_pid_output_t *output);

#endif
