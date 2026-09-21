#ifndef OPENFLIGHTCOMPUTER_MANUAL_EASY_BEHAVIOR_H
#define OPENFLIGHTCOMPUTER_MANUAL_EASY_BEHAVIOR_H

#include "control_input_shaping.h"
#include "flight_behavior_result.h"
#include "flight_control_core.h"
#include "imu_sample.h"
#include "receiver_failsafe.h"
#include "takeoff_leveling.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    takeoff_leveling_t takeoff_leveling;
    control_setpoint_t last_setpoint;
    bool initialized;
} manual_easy_behavior_t;

bool manual_easy_behavior_initialize(manual_easy_behavior_t *behavior,
                                     const easy_mode_config_t *easy_mode);
void manual_easy_behavior_reset(manual_easy_behavior_t *behavior,
                                const easy_mode_config_t *easy_mode);
flight_behavior_result_t manual_easy_behavior_update(
    manual_easy_behavior_t *behavior,
    const prepared_control_input_shaping_t *control,
    const easy_mode_config_t *easy_mode,
    const receiver_failsafe_decision_t *decision,
    const vehicle_state_t *vehicle_state,
    imu_freshness_t imu_freshness,
    uint64_t now_us,
    control_objective_t *objective);

#endif
