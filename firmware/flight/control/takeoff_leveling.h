#ifndef OPENFLIGHTCOMPUTER_TAKEOFF_LEVELING_H
#define OPENFLIGHTCOMPUTER_TAKEOFF_LEVELING_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TAKEOFF_LEVELING_DISABLED = 0,
    TAKEOFF_LEVELING_CAPTURING,
    TAKEOFF_LEVELING_ACTIVE,
    TAKEOFF_LEVELING_COMPLETE,
} takeoff_leveling_state_t;

typedef struct {
    uint16_t armed_idle_permille;
    uint16_t activation_throttle_permille;
    uint16_t leveling_rate_decidegrees_per_second;
    bool takeoff_leveling_enabled;
} easy_mode_config_t;

typedef struct {
    float launch_roll_offset_degrees;
    float launch_pitch_offset_degrees;
    float effective_roll_degrees;
    float effective_pitch_degrees;
    uint64_t last_sample_at_us;
    takeoff_leveling_state_t state;
    bool initialized;
} takeoff_leveling_t;

bool easy_mode_config_is_valid(const easy_mode_config_t *config);
float easy_mode_armed_idle(const easy_mode_config_t *config);
float easy_mode_activation_throttle(const easy_mode_config_t *config);
void takeoff_leveling_initialize(takeoff_leveling_t *leveling);
void takeoff_leveling_reset(takeoff_leveling_t *leveling,
                            const easy_mode_config_t *config);
bool takeoff_leveling_apply(takeoff_leveling_t *leveling,
                            const easy_mode_config_t *config,
                            float throttle,
                            float pilot_roll_degrees,
                            float pilot_pitch_degrees,
                            float measured_roll_degrees,
                            float measured_pitch_degrees,
                            uint64_t sample_at_us,
                            float *effective_roll_degrees,
                            float *effective_pitch_degrees);
void takeoff_leveling_complete(takeoff_leveling_t *leveling,
                               float pilot_roll_degrees,
                               float pilot_pitch_degrees);

#endif
