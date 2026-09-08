#include "receiver_failsafe.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

static receiver_control_snapshot_t control_at(uint64_t timestamp_us,
                                              float throttle,
                                              bool arm_switch_high)
{
    return (receiver_control_snapshot_t){
        .roll = 0.25F,
        .pitch = -0.5F,
        .yaw = 0.75F,
        .throttle = throttle,
        .received_at_us = timestamp_us,
        .source_sequence = 7U,
        .arm_switch_high = arm_switch_high,
        .valid = true,
    };
}

static receiver_failsafe_t initialized_policy(uint64_t now_us)
{
    receiver_failsafe_config_t config;
    receiver_failsafe_t failsafe;

    receiver_failsafe_default_config(&config);
    assert(receiver_failsafe_initialize(&failsafe, &config, now_us));
    return failsafe;
}

static void assert_update(receiver_failsafe_t *failsafe,
                          const receiver_control_snapshot_t *control,
                          uint64_t now_us,
                          receiver_failsafe_state_t expected_state,
                          receiver_failsafe_action_t expected_action)
{
    receiver_failsafe_decision_t decision;

    assert(receiver_failsafe_update(failsafe, control, now_us, &decision));
    assert(decision.state == expected_state);
    assert(decision.action == expected_action);
}

static void test_default_config_and_invalid_configs(void)
{
    receiver_failsafe_config_t config;
    receiver_failsafe_t failsafe;

    receiver_failsafe_default_config(&config);
    assert(receiver_failsafe_config_is_valid(&config));
    assert(config.stale_after_us == UINT64_C(25000));
    assert(config.loss_detected_after_us == UINT64_C(100000));
    assert(config.hold_last_until_us == UINT64_C(400000));
    assert(config.stage_two_after_us == UINT64_C(1500000));
    assert(config.stage_one_throttle == 0.05F);

    config.hold_last_until_us = config.loss_detected_after_us;
    assert(!receiver_failsafe_config_is_valid(&config));
    assert(!receiver_failsafe_initialize(&failsafe, &config, 0U));

    receiver_failsafe_default_config(&config);
    config.stage_one_roll = 1.01F;
    assert(!receiver_failsafe_config_is_valid(&config));

    receiver_failsafe_default_config(&config);
    config.stage_one_throttle = NAN;
    assert(!receiver_failsafe_config_is_valid(&config));
}

static void test_timing_boundaries_and_requested_controls(void)
{
    receiver_failsafe_t failsafe = initialized_policy(0U);
    receiver_control_snapshot_t control = control_at(1000U, 0.9F, true);
    receiver_failsafe_decision_t decision;

    assert_update(&failsafe,
                  &control,
                  26000U,
                  RECEIVER_FAILSAFE_LIVE,
                  RECEIVER_FAILSAFE_ACTION_LIVE);
    assert_update(&failsafe,
                  &control,
                  26001U,
                  RECEIVER_FAILSAFE_STALE_HOLD,
                  RECEIVER_FAILSAFE_ACTION_HOLD_LAST);
    assert_update(&failsafe,
                  &control,
                  101000U,
                  RECEIVER_FAILSAFE_STALE_HOLD,
                  RECEIVER_FAILSAFE_ACTION_HOLD_LAST);
    assert_update(&failsafe,
                  &control,
                  101001U,
                  RECEIVER_FAILSAFE_LOSS_HOLD,
                  RECEIVER_FAILSAFE_ACTION_HOLD_LAST);
    assert_update(&failsafe,
                  &control,
                  401000U,
                  RECEIVER_FAILSAFE_LOSS_HOLD,
                  RECEIVER_FAILSAFE_ACTION_HOLD_LAST);
    assert(receiver_failsafe_update(&failsafe, &control, 401001U, &decision));
    assert(decision.state == RECEIVER_FAILSAFE_STAGE_ONE);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_STAGE_ONE);
    assert(decision.requested_control.roll == 0.0F);
    assert(decision.requested_control.pitch == 0.0F);
    assert(decision.requested_control.yaw == 0.0F);
    assert(decision.requested_control.throttle == 0.05F);
    assert(!decision.requested_control.arm_switch_high);
    assert_update(&failsafe,
                  &control,
                  1501000U,
                  RECEIVER_FAILSAFE_STAGE_ONE,
                  RECEIVER_FAILSAFE_ACTION_STAGE_ONE);
    assert_update(&failsafe,
                  &control,
                  1501001U,
                  RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
                  RECEIVER_FAILSAFE_ACTION_STOP);
}

static void test_short_loss_recovers_automatically(void)
{
    receiver_failsafe_t failsafe = initialized_policy(0U);
    receiver_control_snapshot_t old = control_at(0U, 1.0F, true);
    receiver_control_snapshot_t recovered = control_at(450000U, 0.7F, true);
    receiver_failsafe_decision_t decision;

    assert_update(&failsafe,
                  &old,
                  450000U,
                  RECEIVER_FAILSAFE_STAGE_ONE,
                  RECEIVER_FAILSAFE_ACTION_STAGE_ONE);
    assert(receiver_failsafe_update(
        &failsafe, &recovered, 450000U, &decision));
    assert(decision.state == RECEIVER_FAILSAFE_LIVE);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_LIVE);
    assert(decision.requested_control.throttle == 0.7F);
    assert(!decision.stage_two_latched);
}

static void test_stage_two_requires_stable_safe_recovery_and_release(void)
{
    receiver_failsafe_t failsafe = initialized_policy(0U);
    receiver_control_snapshot_t old = control_at(0U, 0.8F, true);
    receiver_control_snapshot_t unsafe = control_at(1600000U, 0.3F, false);
    receiver_control_snapshot_t safe = control_at(1700000U, 0.01F, false);
    receiver_failsafe_decision_t decision;

    assert_update(&failsafe,
                  &old,
                  1600000U,
                  RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    assert_update(&failsafe,
                  &unsafe,
                  1600000U,
                  RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    assert(!receiver_failsafe_release_stage_two(&failsafe));

    assert_update(&failsafe,
                  &safe,
                  1700000U,
                  RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    safe.received_at_us = 2190000U;
    assert(receiver_failsafe_update(&failsafe, &safe, 2190000U, &decision));
    assert(!decision.recovery_ready);
    safe.received_at_us = 2200000U;
    assert(receiver_failsafe_update(&failsafe, &safe, 2200000U, &decision));
    assert(decision.recovery_ready);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_STOP);
    assert(receiver_failsafe_release_stage_two(&failsafe));
    assert_update(&failsafe,
                  &safe,
                  2200000U,
                  RECEIVER_FAILSAFE_LIVE,
                  RECEIVER_FAILSAFE_ACTION_LIVE);
}

static void test_unsafe_recovery_resets_the_stability_timer(void)
{
    receiver_failsafe_t failsafe = initialized_policy(0U);
    receiver_control_snapshot_t old = control_at(0U, 0.8F, true);
    receiver_control_snapshot_t safe = control_at(1600001U, 0.0F, false);
    receiver_control_snapshot_t armed = control_at(1800000U, 0.0F, true);

    assert_update(&failsafe,
                  &old,
                  1600000U,
                  RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    assert_update(&failsafe,
                  &safe,
                  1600001U,
                  RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    assert_update(&failsafe,
                  &armed,
                  1800000U,
                  RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
                  RECEIVER_FAILSAFE_ACTION_STOP);
    assert(!receiver_failsafe_release_stage_two(&failsafe));
}

static void test_unavailable_and_clock_rollback_fail_closed(void)
{
    receiver_failsafe_t failsafe = initialized_policy(1000U);
    receiver_failsafe_decision_t decision;
    receiver_control_snapshot_t future = control_at(2000U, 0.0F, false);

    assert(receiver_failsafe_update(&failsafe, NULL, 101001U, &decision));
    assert(decision.state == RECEIVER_FAILSAFE_UNAVAILABLE);
    assert(decision.receiver_loss_detected);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_NONE);

    assert(receiver_failsafe_update(&failsafe, NULL, 1501001U, &decision));
    assert(decision.state == RECEIVER_FAILSAFE_STAGE_TWO_LATCHED);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_STOP);

    assert(receiver_failsafe_update(&failsafe, &future, 1999U, &decision));
    assert(!decision.clock_valid);
    assert(decision.stage_two_latched);
    assert(decision.action == RECEIVER_FAILSAFE_ACTION_STOP);
}

int main(void)
{
    test_default_config_and_invalid_configs();
    test_timing_boundaries_and_requested_controls();
    test_short_loss_recovers_automatically();
    test_stage_two_requires_stable_safe_recovery_and_release();
    test_unsafe_recovery_resets_the_stability_timer();
    test_unavailable_and_clock_rollback_fail_closed();
    return 0;
}
