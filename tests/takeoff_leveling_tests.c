#include "takeoff_leveling.h"

#include <assert.h>
#include <math.h>

static bool close_to(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001F;
}

int main(void)
{
    const easy_mode_config_t config = {
        .armed_idle_permille = 50U,
        .activation_throttle_permille = 180U,
        .leveling_rate_decidegrees_per_second = 100U,
        .takeoff_leveling_enabled = true,
    };
    takeoff_leveling_t leveling;
    float roll;
    float pitch;

    assert(easy_mode_config_is_valid(&config));
    assert(close_to(easy_mode_armed_idle(&config), 0.05F));
    assert(close_to(easy_mode_activation_throttle(&config), 0.18F));

    takeoff_leveling_initialize(&leveling);
    takeoff_leveling_reset(&leveling, &config);
    assert(takeoff_leveling_apply(&leveling, &config, 0.0F,
                                  0.0F, 0.0F, 3.0F, 8.0F, 1000U,
                                  &roll, &pitch));
    assert(leveling.state == TAKEOFF_LEVELING_CAPTURING);
    assert(close_to(roll, 3.0F));
    assert(close_to(pitch, 8.0F));

    /* First nonzero throttle freezes the reference and starts leveling. */
    assert(takeoff_leveling_apply(&leveling, &config, 0.17F,
                                  -2.0F, 1.0F, 4.0F, 7.0F, 2000U,
                                  &roll, &pitch));
    assert(leveling.state == TAKEOFF_LEVELING_ACTIVE);
    assert(close_to(roll, 4.0F));
    assert(close_to(pitch, 7.0F));

    /* Leveling proceeds below the legacy activation threshold. */
    assert(takeoff_leveling_apply(&leveling, &config, 0.10F,
                                  -2.0F, 1.0F, -12.0F, -15.0F, 102000U,
                                  &roll, &pitch));
    assert(leveling.state == TAKEOFF_LEVELING_ACTIVE);
    assert(close_to(roll, 3.0F));
    assert(close_to(pitch, 6.0F));

    /* Returning to zero pauses leveling and preserves the current reference. */
    assert(takeoff_leveling_apply(&leveling, &config, 0.0F,
                                  0.0F, 0.0F, 20.0F, 20.0F, 202000U,
                                  &roll, &pitch));
    assert(leveling.state == TAKEOFF_LEVELING_ACTIVE);
    assert(close_to(roll, 5.0F));
    assert(close_to(pitch, 5.0F));

    /* Ten degrees per second removes another degree in 100 ms. */
    assert(takeoff_leveling_apply(&leveling, &config, 0.2F,
                                  -2.0F, 1.0F, 3.0F, 6.0F, 202000U,
                                  &roll, &pitch));
    assert(close_to(leveling.launch_roll_offset_degrees, 4.0F));
    assert(close_to(leveling.launch_pitch_offset_degrees, 4.0F));
    assert(close_to(roll, 2.0F));
    assert(close_to(pitch, 5.0F));

    /* Exact zero pauses an active transition. */
    assert(takeoff_leveling_apply(&leveling, &config, 0.0F,
                                  0.0F, 0.0F, 3.0F, 6.0F, 303000U,
                                  &roll, &pitch));
    assert(close_to(leveling.launch_roll_offset_degrees, 4.0F));
    assert(close_to(leveling.launch_pitch_offset_degrees, 4.0F));

    assert(takeoff_leveling_apply(&leveling, &config, 0.2F,
                                  0.0F, 0.0F, 0.0F, 0.0F, 703000U,
                                  &roll, &pitch));
    assert(leveling.state == TAKEOFF_LEVELING_COMPLETE);
    assert(close_to(roll, 0.0F));
    assert(close_to(pitch, 0.0F));
    return 0;
}
