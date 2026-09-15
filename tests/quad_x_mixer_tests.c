#include "quad_x_mixer.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.00001F;
}

static float average_motor_throttle(const quad_x_mixer_output_t *output)
{
    float total = 0.0F;
    size_t motor;

    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        total += output->command.throttle[motor];
    }
    return total / (float)MOTOR_COMMAND_MOTOR_COUNT;
}

static void all_bounded_cases_preserve_collective_ceiling(void)
{
    static const float throttles[] = {
        0.0F, 0.001F, 0.002F, 0.01F, 0.1F, 0.5F, 0.9F, 1.0F,
    };
    static const float corrections[] = {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F};
    quad_x_mixer_output_t output;
    size_t throttle_index;
    size_t roll_index;
    size_t pitch_index;
    size_t yaw_index;

    for (throttle_index = 0U;
         throttle_index < sizeof(throttles) / sizeof(throttles[0]);
         throttle_index++) {
        for (roll_index = 0U;
             roll_index < sizeof(corrections) / sizeof(corrections[0]);
             roll_index++) {
            for (pitch_index = 0U;
                 pitch_index < sizeof(corrections) / sizeof(corrections[0]);
                 pitch_index++) {
                for (yaw_index = 0U;
                     yaw_index < sizeof(corrections) / sizeof(corrections[0]);
                     yaw_index++) {
                    const float correction[3] = {
                        corrections[roll_index],
                        corrections[pitch_index],
                        corrections[yaw_index],
                    };
                    size_t motor;

                    assert(quad_x_mixer_apply(
                        PROPELLER_LAYOUT_PROPS_IN,
                        throttles[throttle_index],
                        correction,
                        1U,
                        &output));
                    assert(output.collective_shift == 0.0F);
                    assert(average_motor_throttle(&output) <=
                           throttles[throttle_index] + 0.00001F);
                    for (motor = 0U;
                         motor < MOTOR_COMMAND_MOTOR_COUNT;
                         motor++) {
                        assert(output.command.throttle[motor] >= 0.0F);
                        assert(output.command.throttle[motor] <= 1.0F);
                    }
                }
            }
        }
    }
}

int main(void)
{
    const float correction[3] = {0.10F, -0.05F, 0.02F};
    quad_x_mixer_output_t output;
    prepared_quad_x_mixer_t prepared;

    assert(quad_x_mixer_prepare(PROPELLER_LAYOUT_PROPS_IN, &prepared));
    assert(quad_x_mixer_apply_prepared(&prepared, 0.5F, correction, 10U,
                                       &output));
    assert(close_to(output.command.throttle[0], 0.57F));
    assert(close_to(output.command.throttle[1], 0.63F));
    assert(close_to(output.command.throttle[2], 0.33F));
    assert(close_to(output.command.throttle[3], 0.47F));
    assert(!output.saturated);
    assert(output.correction_scale == 1.0F);
    assert(output.collective_shift == 0.0F);

    assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_OUT, 0.5F,
                              correction, 11U, &output));
    assert(close_to(output.command.throttle[0], 0.53F));
    assert(close_to(output.command.throttle[1], 0.67F));
    assert(close_to(output.command.throttle[2], 0.37F));
    assert(close_to(output.command.throttle[3], 0.43F));

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
        assert(output.collective_shift == 0.0F);
        assert(close_to(average_motor_throttle(&output), 0.9F));
        for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            assert(output.command.throttle[motor] >= 0.0F);
            assert(output.command.throttle[motor] <= 1.0F);
        }
    }
    {
        const float upper_limit_correction[3] = {0.1F, 0.1F, 0.0F};
        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.95F,
                                  upper_limit_correction, 14U, &output));
        assert(output.saturated);
        assert(close_to(output.correction_scale, 0.25F));
        assert(output.collective_shift == 0.0F);
        assert(close_to(output.command.throttle[0], 1.0F));
        assert(close_to(output.command.throttle[3], 0.9F));
        assert(close_to(average_motor_throttle(&output), 0.95F));
    }
    {
        const float positive_pitch[3] = {0.0F, 0.1F, 0.0F};

        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.5F,
                                  positive_pitch, 15U, &output));
        assert(output.command.throttle[0] > output.command.throttle[1]);
        assert(output.command.throttle[2] > output.command.throttle[3]);
    }
    {
        /* Regression for the captured low-throttle bench acceleration. */
        const float captured_correction[3] = {0.051F, 0.004F, 0.0F};
        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.002F,
                                  captured_correction, 16U, &output));
        assert(output.saturated);
        assert(output.collective_shift == 0.0F);
        assert(average_motor_throttle(&output) <= 0.002F);
        for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            assert(output.command.throttle[motor] <= 0.0041F);
        }
    }

    {
        const float invalid[3] = {NAN, 0.0F, 0.0F};
        assert(quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.0F,
                                  invalid, 17U, &output));
        for (size_t motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
            assert(output.command.throttle[motor] == 0.0F);
        }
        assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.5F,
                                   invalid, 17U, &output));
    }
    {
        const float invalid[3] = {1.01F, 0.0F, 0.0F};
        assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 0.5F,
                                   invalid, 17U, &output));
    }
    assert(!quad_x_mixer_prepare(PROPELLER_LAYOUT_COUNT, &prepared));
    assert(!quad_x_mixer_apply(PROPELLER_LAYOUT_PROPS_IN, 1.1F,
                               correction, 17U, &output));
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_IN),
                  "PROPS_IN") == 0);
    assert(strcmp(propeller_layout_name(PROPELLER_LAYOUT_PROPS_OUT),
                  "PROPS_OUT") == 0);
    all_bounded_cases_preserve_collective_ceiling();
    return 0;
}
