#include "pitch_attitude_controller.h"
#include "roll_attitude_controller.h"
#include "yaw_attitude_controller.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

int main(void)
{
    roll_attitude_controller_config_t roll = {.gain_per_s = 4.0F};
    pitch_attitude_controller_config_t pitch = {.gain_per_s = 4.0F};
    float desired_rate_dps;

    assert(roll_attitude_controller_config_is_valid(&roll));
    assert(roll_attitude_controller_update(&roll, 30.0F, 5.0F, 180.0F,
                                           &desired_rate_dps));
    assert(fabsf(desired_rate_dps - 100.0F) < 0.000001F);
    assert(roll_attitude_controller_update(&roll, 100.0F, 0.0F, 180.0F,
                                           &desired_rate_dps));
    assert(desired_rate_dps == 180.0F);

    assert(pitch_attitude_controller_config_is_valid(&pitch));
    assert(pitch_attitude_controller_update(&pitch, -20.0F, -5.0F,
                                            180.0F, &desired_rate_dps));
    assert(fabsf(desired_rate_dps + 60.0F) < 0.000001F);

    assert(yaw_attitude_controller_update(75.0F, 150.0F,
                                          &desired_rate_dps));
    assert(desired_rate_dps == 75.0F);
    assert(yaw_attitude_controller_update(-200.0F, 150.0F,
                                          &desired_rate_dps));
    assert(desired_rate_dps == -150.0F);

    roll.gain_per_s = 0.0F;
    assert(!roll_attitude_controller_config_is_valid(&roll));
    assert(!roll_attitude_controller_update(&roll, 0.0F, 0.0F, 180.0F,
                                            &desired_rate_dps));
    assert(!pitch_attitude_controller_update(NULL, 0.0F, 0.0F, 180.0F,
                                             &desired_rate_dps));
    assert(!yaw_attitude_controller_update(NAN, 150.0F,
                                           &desired_rate_dps));
    return 0;
}
