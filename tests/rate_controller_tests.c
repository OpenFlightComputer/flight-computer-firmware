#include "rate_controller.h"

#include <assert.h>
#include <math.h>

static rate_controller_config_t config(void)
{
    rate_pid_config_t axis = {
        .kp = 0.01F,
        .ki = 0.001F,
        .kd = 0.0F,
        .integral_limit = 0.1F,
        .output_limit = 0.3F,
    };

    return (rate_controller_config_t){
        .type = RATE_CONTROLLER_TYPE_PID,
        .maximum_gap_us = 10000U,
        .axis = {axis, axis, axis},
    };
}

int main(void)
{
    const float desired[3] = {10.0F, -5.0F, 1.0F};
    const float measured[3] = {2.0F, -1.0F, 0.0F};
    rate_controller_config_t source = config();
    rate_controller_t controller;
    rate_controller_output_t output;

    assert(rate_controller_config_is_valid(&source));
    assert(rate_controller_initialize(&controller, &source));
    assert(rate_controller_process(&controller, desired, measured, 1000U, 1U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_SEEDED);
    assert(!output.valid);
    assert(rate_controller_process(&controller, desired, measured, 2000U, 2U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_UPDATED);
    assert(output.valid);
    assert(output.dt_us == 1000U);

    assert(rate_controller_process(&controller, desired, measured, 2000U, 2U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_NO_NEW_SAMPLE);
    assert(!output.valid);

    assert(rate_controller_process(&controller, desired, measured, 20000U, 3U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_CONTINUITY_LOST);
    assert(controller.sample_seeded);
    assert(controller.axis[0].integral == 0.0F);

    assert(rate_controller_process(&controller, desired, measured, 21000U, 4U,
                                   false, &output) ==
           RATE_CONTROLLER_RESULT_DISABLED);
    assert(!controller.sample_seeded);
    assert(rate_controller_type_name(RATE_CONTROLLER_TYPE_PID)[0] == 'P');

    assert(rate_controller_process(&controller, desired, measured, 30000U, 5U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_SEEDED);
    assert(rate_controller_process(&controller, desired, measured, 29000U, 6U,
                                   true, &output) ==
           RATE_CONTROLLER_RESULT_CONTINUITY_LOST);
    assert(!controller.sample_seeded);
    {
        const float invalid[3] = {NAN, 0.0F, 0.0F};

        assert(rate_controller_process(&controller, desired, invalid, 31000U,
                                       7U, true, &output) ==
               RATE_CONTROLLER_RESULT_INVALID_INPUT);
        assert(!controller.sample_seeded);
    }

    source.maximum_gap_us = 0U;
    assert(!rate_controller_config_is_valid(&source));
    return 0;
}
