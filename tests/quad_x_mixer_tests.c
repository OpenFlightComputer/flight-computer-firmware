#include "quad_x_mixer.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

int main(void)
{
    const float correction[3] = {0.10F, -0.05F, 0.02F};
    quad_x_mixer_output_t output;
    prepared_quad_x_mixer_t prepared;

    assert(quad_x_mixer_prepare(PROPELLER_LAYOUT_PROPS_IN, &prepared));
    assert(quad_x_mixer_apply_prepared(&prepared, 0.5F, correction, 10U,
                                       &output));
    assert(close_to(output.command.throttle[0], 0.67F));
    assert(close_to(output.command.throttle[1], 0.53F));
    assert(close_to(output.command.throttle[2], 0.43F));
    assert(close_to(output.command.throttle[3], 0.37F));
    assert(!output.saturated);
    assert(output.correction_scale == 1.0F);
    assert(output.collective_shift == 0.0F);

    assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_OUT, 0.5F,
                              correction, 11U, &output));
    assert(close_to(output.command.throttle[0], 0.63F));
    assert(close_to(output.command.throttle[1], 0.57F));
    assert(close_to(output.command.throttle[2], 0.47F));
    assert(close_to(output.command.throttle[3], 0.33F));

    assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.0F,
                              correction, 12U, &output));
    for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        assert(output.command.throttle[motor] == 0.0F);
    }

    {
        const float high_correction[3] = {0.8F, -0.8F, 0.8F};
        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.9F,
                                  high_correction, 13U, &output));
        assert(output.saturated);
        assert(output.correction_scale < 1.0F);
        for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            assert(output.command.throttle[motor] >= 0.0F);
            assert(output.command.throttle[motor] <= 1.0F);
        }
    }
    {
        const float shift_correction[3] = {0.1F, 0.1F, 0.0F};
        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.95F,
                                  shift_correction, 14U, &output));
        assert(output.saturated);
        assert(close_to(output.collective_shift, -0.15F));
        assert(close_to(output.command.throttle[1], 1.0F));
        assert(close_to(output.command.throttle[2], 0.6F));
    }

    {
        const float invalid[3] = {NAN, 0.0F, 0.0F};
        assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.5F,
                                   invalid, 15U, &output));
    }
    {
        const float invalid[3] = {1.01F, 0.0F, 0.0F};
        assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.5F,
                                   invalid, 15U, &output));
    }
    assert(!quad_x_mixer_prepare(PROPELLER_LAYOUT_COUNT, &prepared));
    assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 1.1F,
                               correction, 15U, &output));
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_IN),
                  "PROPS_IN") == 0);
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_OUT),
                  "PROPS_OUT") == 0);
    return 0;
}
