#include "rate_pid.h"

#include <math.h>
#include <stddef.h>

#define RATE_PID_MAXIMUM_GAIN 10.0F

static float clamp(float value, float limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

bool rate_pid_config_is_valid(const rate_pid_config_t *config)
{
    return (config != NULL) && isfinite(config->kp) &&
           isfinite(config->ki) && isfinite(config->kd) &&
           isfinite(config->integral_limit) &&
           isfinite(config->output_limit) && (config->kp >= 0.0F) &&
           (config->kp <= RATE_PID_MAXIMUM_GAIN) && (config->ki >= 0.0F) &&
           (config->ki <= RATE_PID_MAXIMUM_GAIN) && (config->kd >= 0.0F) &&
           (config->kd <= RATE_PID_MAXIMUM_GAIN) &&
           (config->integral_limit >= 0.0F) &&
           (config->integral_limit <= config->output_limit) &&
           (config->output_limit > 0.0F) && (config->output_limit <= 1.0F);
}

bool rate_pid_initialize(rate_pid_t *pid, const rate_pid_config_t *config)
{
    if ((pid == NULL) || !rate_pid_config_is_valid(config)) {
        return false;
    }
    *pid = (rate_pid_t){
        .config = *config,
        .initialized = true,
    };
    return true;
}

void rate_pid_reset(rate_pid_t *pid)
{
    if ((pid == NULL) || !pid->initialized) {
        return;
    }
    pid->integral = 0.0F;
    pid->previous_measurement = 0.0F;
    pid->measurement_seeded = false;
}

bool rate_pid_seed_measurement(rate_pid_t *pid, float measurement)
{
    if ((pid == NULL) || !pid->initialized || !isfinite(measurement)) {
        return false;
    }
    pid->previous_measurement = measurement;
    pid->measurement_seeded = true;
    return true;
}

bool rate_pid_update(rate_pid_t *pid,
                     float desired_rate,
                     float measured_rate,
                     float dt_seconds,
                     rate_pid_output_t *output)
{
    float error;
    float candidate_integral;
    float candidate_total;
    bool drives_further_into_saturation;

    if ((pid == NULL) || (output == NULL) || !pid->initialized ||
        !pid->measurement_seeded || !isfinite(desired_rate) ||
        !isfinite(measured_rate) || !isfinite(dt_seconds) ||
        (dt_seconds <= 0.0F)) {
        return false;
    }

    error = desired_rate - measured_rate;
    *output = (rate_pid_output_t){
        .proportional = pid->config.kp * error,
        .integral = pid->integral,
        .derivative = -pid->config.kd *
                      ((measured_rate - pid->previous_measurement) /
                      dt_seconds),
    };
    if (!isfinite(error) || !isfinite(output->proportional) ||
        !isfinite(output->derivative)) {
        return false;
    }
    candidate_integral = clamp(pid->integral +
                                   (pid->config.ki * error * dt_seconds),
                               pid->config.integral_limit);
    candidate_total = output->proportional + candidate_integral +
                      output->derivative;
    if (!isfinite(candidate_integral) || !isfinite(candidate_total)) {
        return false;
    }
    drives_further_into_saturation =
        ((candidate_total > pid->config.output_limit) && (error > 0.0F)) ||
        ((candidate_total < -pid->config.output_limit) && (error < 0.0F));
    if (!drives_further_into_saturation) {
        pid->integral = candidate_integral;
    }
    output->integral = pid->integral;
    output->total = clamp(output->proportional + output->integral +
                              output->derivative,
                          pid->config.output_limit);
    pid->previous_measurement = measured_rate;
    return isfinite(output->total);
}
