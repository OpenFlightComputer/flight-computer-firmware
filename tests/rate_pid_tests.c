#include "rate_pid.h"

#include <assert.h>
#include <math.h>

static bool near(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    rate_pid_t pid;
    rate_pid_output_t output;
    rate_pid_config_t config = {
        .kp = 0.1F,
        .ki = 0.2F,
        .kd = 0.01F,
        .integral_limit = 0.2F,
        .output_limit = 0.5F,
    };

    assert(rate_pid_config_is_valid(&config));
    assert(rate_pid_initialize(&pid, &config));
    assert(rate_pid_seed_measurement(&pid, 10.0F));
    assert(rate_pid_update(&pid, 12.0F, 11.0F, 0.01F, &output));
    assert(near(output.proportional, 0.1F));
    assert(near(output.derivative, -1.0F));
    assert(near(output.integral, 0.002F));
    assert(near(output.total, -0.5F));

    /* A setpoint step alone has no derivative kick. */
    assert(rate_pid_update(&pid, 20.0F, 11.0F, 0.01F, &output));
    assert(near(output.derivative, 0.0F));

    /* Positive error cannot wind the integral farther into saturation. */
    config = (rate_pid_config_t){
        .kp = 1.0F,
        .ki = 1.0F,
        .kd = 0.0F,
        .integral_limit = 0.2F,
        .output_limit = 0.3F,
    };
    assert(rate_pid_initialize(&pid, &config));
    assert(rate_pid_seed_measurement(&pid, 0.0F));
    assert(rate_pid_update(&pid, 1.0F, 0.0F, 0.1F, &output));
    assert(near(output.integral, 0.0F));
    assert(near(output.total, 0.3F));

    config = (rate_pid_config_t){
        .kp = 0.0F,
        .ki = 1.0F,
        .kd = 0.0F,
        .integral_limit = 0.2F,
        .output_limit = 1.0F,
    };
    assert(rate_pid_initialize(&pid, &config));
    assert(rate_pid_seed_measurement(&pid, 0.0F));
    assert(rate_pid_update(&pid, 1.0F, 0.0F, 1.0F, &output));
    assert(near(output.integral, 0.2F));

    rate_pid_reset(&pid);
    assert(!pid.measurement_seeded);
    assert(near(pid.integral, 0.0F));

    config.output_limit = 0.0F;
    assert(!rate_pid_config_is_valid(&config));
    return 0;
}
