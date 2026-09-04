#ifndef OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_RGB_LED_SAFE_STATE_H
#define OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_RGB_LED_SAFE_STATE_H

#include <stdint.h>

/*
 * Establish a deterministic off state without assigning status meaning to the
 * LED. The core frequency must already be configured and verified.
 */
void flightcomputer_v1_rgb_led_force_off(uint32_t core_clock_hz);

#endif
