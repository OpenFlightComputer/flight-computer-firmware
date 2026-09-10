#ifndef OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_RGB_LED_H
#define OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_RGB_LED_H

#include <stdint.h>

#include <stdbool.h>

bool flightcomputer_v1_rgb_led_initialize(uint32_t core_clock_hz);
bool flightcomputer_v1_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue);

#endif
