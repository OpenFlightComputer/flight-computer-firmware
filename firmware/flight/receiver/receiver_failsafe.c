#include "receiver_failsafe.h"

#include <limits.h>
#include <stddef.h>

#define RECEIVER_FAILSAFE_DEFAULT_STALE_AFTER_US UINT64_C(25000)
#define RECEIVER_FAILSAFE_DEFAULT_LOSS_DETECTED_AFTER_US UINT64_C(100000)
#define RECEIVER_FAILSAFE_DEFAULT_HOLD_LAST_UNTIL_US UINT64_C(400000)
#define RECEIVER_FAILSAFE_DEFAULT_STAGE_TWO_AFTER_US UINT64_C(1500000)
#define RECEIVER_FAILSAFE_DEFAULT_RECOVERY_STABLE_US UINT64_C(500000)
#define RECEIVER_FAILSAFE_DEFAULT_STAGE_ONE_THROTTLE 0.05F
#define RECEIVER_FAILSAFE_DEFAULT_RECOVERY_THROTTLE_MAXIMUM 0.05F

static bool axis_is_valid(float value)
{
    return (value >= -1.0F) && (value <= 1.0F);
}

static bool throttle_is_valid(float value)
{
    return (value >= 0.0F) && (value <= 1.0F);
}

static void set_state(receiver_failsafe_t *failsafe,
                      receiver_failsafe_state_t state)
{
    if (failsafe->state == state) {
        return;
    }

    failsafe->state = state;
    if (failsafe->transition_count != UINT32_MAX) {
        failsafe->transition_count++;
    }
}

static receiver_control_snapshot_t stopped_control(void)
{
    return (receiver_control_snapshot_t){
        .valid = true,
    };
}

static receiver_control_snapshot_t stage_one_control(
    const receiver_failsafe_config_t *config)
{
    return (receiver_control_snapshot_t){
        .roll = config->stage_one_roll,
        .pitch = config->stage_one_pitch,
        .yaw = config->stage_one_yaw,
        .throttle = config->stage_one_throttle,
        .arm_switch_high = false,
        .valid = true,
    };
}

static void latch_stage_two(receiver_failsafe_t *failsafe)
{
    failsafe->stage_two_latched = true;
    failsafe->recovery_timer_active = false;
    failsafe->recovery_ready = false;
    set_state(failsafe, RECEIVER_FAILSAFE_STAGE_TWO_LATCHED);
}

static void update_stage_two_recovery(
    receiver_failsafe_t *failsafe,
    const receiver_control_snapshot_t *control,
    uint64_t now_us,
    bool control_is_fresh)
{
    const bool safe_controls =
        control_is_fresh && !control->arm_switch_high &&
        (control->throttle <= failsafe->config.recovery_throttle_maximum);

    if (!safe_controls) {
        failsafe->recovery_timer_active = false;
        failsafe->recovery_ready = false;
        set_state(failsafe, RECEIVER_FAILSAFE_STAGE_TWO_LATCHED);
        return;
    }

    if (!failsafe->recovery_timer_active) {
        failsafe->recovery_timer_active = true;
        failsafe->recovery_started_at_us = now_us;
        set_state(failsafe, RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING);
        return;
    }

    if (now_us < failsafe->recovery_started_at_us) {
        latch_stage_two(failsafe);
        return;
    }

    failsafe->recovery_ready =
        (now_us - failsafe->recovery_started_at_us) >=
        failsafe->config.recovery_stable_us;
}

void receiver_failsafe_default_config(receiver_failsafe_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (receiver_failsafe_config_t){
        .stale_after_us = RECEIVER_FAILSAFE_DEFAULT_STALE_AFTER_US,
        .loss_detected_after_us =
            RECEIVER_FAILSAFE_DEFAULT_LOSS_DETECTED_AFTER_US,
        .hold_last_until_us = RECEIVER_FAILSAFE_DEFAULT_HOLD_LAST_UNTIL_US,
        .stage_two_after_us = RECEIVER_FAILSAFE_DEFAULT_STAGE_TWO_AFTER_US,
        .recovery_stable_us = RECEIVER_FAILSAFE_DEFAULT_RECOVERY_STABLE_US,
        .stage_one_roll = 0.0F,
        .stage_one_pitch = 0.0F,
        .stage_one_yaw = 0.0F,
        .stage_one_throttle = RECEIVER_FAILSAFE_DEFAULT_STAGE_ONE_THROTTLE,
        .recovery_throttle_maximum =
            RECEIVER_FAILSAFE_DEFAULT_RECOVERY_THROTTLE_MAXIMUM,
    };
}

bool receiver_failsafe_config_is_valid(
    const receiver_failsafe_config_t *config)
{
    return (config != NULL) && (config->stale_after_us > 0U) &&
           (config->stale_after_us < config->loss_detected_after_us) &&
           (config->loss_detected_after_us < config->hold_last_until_us) &&
           (config->hold_last_until_us < config->stage_two_after_us) &&
           (config->recovery_stable_us > 0U) &&
           axis_is_valid(config->stage_one_roll) &&
           axis_is_valid(config->stage_one_pitch) &&
           axis_is_valid(config->stage_one_yaw) &&
           throttle_is_valid(config->stage_one_throttle) &&
           throttle_is_valid(config->recovery_throttle_maximum);
}

bool receiver_failsafe_initialize(receiver_failsafe_t *failsafe,
                                  const receiver_failsafe_config_t *config,
                                  uint64_t initialized_at_us)
{
    if (failsafe == NULL) {
        return false;
    }
    *failsafe = (receiver_failsafe_t){0};
    if (!receiver_failsafe_config_is_valid(config)) {
        return false;
    }

    failsafe->config = *config;
    failsafe->state = RECEIVER_FAILSAFE_UNAVAILABLE;
    failsafe->initialized_at_us = initialized_at_us;
    failsafe->initialized = true;
    return true;
}

bool receiver_failsafe_update(receiver_failsafe_t *failsafe,
                              const receiver_control_snapshot_t *control,
                              uint64_t now_us,
                              receiver_failsafe_decision_t *decision)
{
    uint64_t reference_us;
    uint64_t age_us;
    bool control_valid;
    bool clock_valid;

    if ((failsafe == NULL) || !failsafe->initialized ||
        (decision == NULL)) {
        return false;
    }

    control_valid = (control != NULL) && control->valid;
    reference_us = control_valid ? control->received_at_us
                                 : failsafe->initialized_at_us;
    clock_valid = now_us >= reference_us;
    age_us = clock_valid ? now_us - reference_us : UINT64_MAX;

    if (!clock_valid) {
        latch_stage_two(failsafe);
    } else if (failsafe->stage_two_latched) {
        update_stage_two_recovery(
            failsafe,
            control,
            now_us,
            control_valid && (age_us <= failsafe->config.stale_after_us));
    } else if (!control_valid) {
        if (age_us > failsafe->config.stage_two_after_us) {
            latch_stage_two(failsafe);
        } else {
            set_state(failsafe, RECEIVER_FAILSAFE_UNAVAILABLE);
        }
    } else if (age_us <= failsafe->config.stale_after_us) {
        set_state(failsafe, RECEIVER_FAILSAFE_LIVE);
    } else if (age_us <= failsafe->config.loss_detected_after_us) {
        set_state(failsafe, RECEIVER_FAILSAFE_STALE_HOLD);
    } else if (age_us <= failsafe->config.hold_last_until_us) {
        set_state(failsafe, RECEIVER_FAILSAFE_LOSS_HOLD);
    } else if (age_us <= failsafe->config.stage_two_after_us) {
        set_state(failsafe, RECEIVER_FAILSAFE_STAGE_ONE);
    } else {
        latch_stage_two(failsafe);
    }

    *decision = (receiver_failsafe_decision_t){
        .state = failsafe->state,
        .action = RECEIVER_FAILSAFE_ACTION_NONE,
        .requested_control = stopped_control(),
        .link_age_us = age_us,
        .transition_count = failsafe->transition_count,
        .receiver_loss_detected =
            failsafe->stage_two_latched ||
            (age_us > failsafe->config.loss_detected_after_us),
        .stage_two_latched = failsafe->stage_two_latched,
        .recovery_ready = failsafe->recovery_ready,
        .clock_valid = clock_valid,
    };

    switch (failsafe->state) {
    case RECEIVER_FAILSAFE_LIVE:
        decision->action = RECEIVER_FAILSAFE_ACTION_LIVE;
        decision->requested_control = *control;
        break;
    case RECEIVER_FAILSAFE_STALE_HOLD:
    case RECEIVER_FAILSAFE_LOSS_HOLD:
        decision->action = RECEIVER_FAILSAFE_ACTION_HOLD_LAST;
        decision->requested_control = *control;
        break;
    case RECEIVER_FAILSAFE_STAGE_ONE:
        decision->action = RECEIVER_FAILSAFE_ACTION_STAGE_ONE;
        decision->requested_control = stage_one_control(&failsafe->config);
        break;
    case RECEIVER_FAILSAFE_STAGE_TWO_LATCHED:
    case RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING:
        decision->action = RECEIVER_FAILSAFE_ACTION_STOP;
        break;
    case RECEIVER_FAILSAFE_UNAVAILABLE:
    case RECEIVER_FAILSAFE_STATE_COUNT:
    default:
        break;
    }

    return true;
}

bool receiver_failsafe_release_stage_two(receiver_failsafe_t *failsafe)
{
    if ((failsafe == NULL) || !failsafe->initialized ||
        !failsafe->stage_two_latched || !failsafe->recovery_ready) {
        return false;
    }

    failsafe->stage_two_latched = false;
    failsafe->recovery_timer_active = false;
    failsafe->recovery_ready = false;
    set_state(failsafe, RECEIVER_FAILSAFE_UNAVAILABLE);
    return true;
}

const char *receiver_failsafe_state_name(receiver_failsafe_state_t state)
{
    static const char *const names[RECEIVER_FAILSAFE_STATE_COUNT] = {
        "UNAVAILABLE",       "LIVE",      "STALE_HOLD",
        "LOSS_HOLD",        "STAGE_ONE", "STAGE_TWO_LATCHED",
        "STAGE_TWO_RECOVERING",
    };

    if ((unsigned int)state >= RECEIVER_FAILSAFE_STATE_COUNT) {
        return "INVALID";
    }
    return names[state];
}

const char *receiver_failsafe_action_name(receiver_failsafe_action_t action)
{
    static const char *const names[RECEIVER_FAILSAFE_ACTION_COUNT] = {
        "NONE", "LIVE", "HOLD_LAST", "STAGE_ONE", "STOP",
    };

    if ((unsigned int)action >= RECEIVER_FAILSAFE_ACTION_COUNT) {
        return "INVALID";
    }
    return names[action];
}
