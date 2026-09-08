#ifndef OPENFLIGHTCOMPUTER_RECEIVER_FAILSAFE_H
#define OPENFLIGHTCOMPUTER_RECEIVER_FAILSAFE_H

#include "receiver_normalization.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t stale_after_us;
    uint64_t loss_detected_after_us;
    uint64_t hold_last_until_us;
    uint64_t stage_two_after_us;
    uint64_t recovery_stable_us;
    float stage_one_roll;
    float stage_one_pitch;
    float stage_one_yaw;
    float stage_one_throttle;
    float recovery_throttle_maximum;
} receiver_failsafe_config_t;

typedef enum {
    RECEIVER_FAILSAFE_UNAVAILABLE = 0,
    RECEIVER_FAILSAFE_LIVE,
    RECEIVER_FAILSAFE_STALE_HOLD,
    RECEIVER_FAILSAFE_LOSS_HOLD,
    RECEIVER_FAILSAFE_STAGE_ONE,
    RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
    RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING,
    RECEIVER_FAILSAFE_STATE_COUNT,
} receiver_failsafe_state_t;

typedef enum {
    RECEIVER_FAILSAFE_ACTION_NONE = 0,
    RECEIVER_FAILSAFE_ACTION_LIVE,
    RECEIVER_FAILSAFE_ACTION_HOLD_LAST,
    RECEIVER_FAILSAFE_ACTION_STAGE_ONE,
    RECEIVER_FAILSAFE_ACTION_STOP,
    RECEIVER_FAILSAFE_ACTION_COUNT,
} receiver_failsafe_action_t;

typedef struct {
    receiver_failsafe_state_t state;
    receiver_failsafe_action_t action;
    receiver_control_snapshot_t requested_control;
    uint64_t link_age_us;
    uint32_t transition_count;
    bool receiver_loss_detected;
    bool stage_two_latched;
    bool recovery_ready;
    bool clock_valid;
} receiver_failsafe_decision_t;

typedef struct {
    receiver_failsafe_config_t config;
    receiver_failsafe_state_t state;
    uint64_t initialized_at_us;
    uint64_t recovery_started_at_us;
    uint32_t transition_count;
    bool stage_two_latched;
    bool recovery_timer_active;
    bool recovery_ready;
    bool initialized;
} receiver_failsafe_t;

void receiver_failsafe_default_config(receiver_failsafe_config_t *config);
bool receiver_failsafe_config_is_valid(
    const receiver_failsafe_config_t *config);
bool receiver_failsafe_initialize(receiver_failsafe_t *failsafe,
                                  const receiver_failsafe_config_t *config,
                                  uint64_t initialized_at_us);
bool receiver_failsafe_update(receiver_failsafe_t *failsafe,
                              const receiver_control_snapshot_t *control,
                              uint64_t now_us,
                              receiver_failsafe_decision_t *decision);
bool receiver_failsafe_release_stage_two(receiver_failsafe_t *failsafe);
const char *receiver_failsafe_state_name(receiver_failsafe_state_t state);
const char *receiver_failsafe_action_name(receiver_failsafe_action_t action);

#endif
