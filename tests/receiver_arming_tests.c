#include "receiver_arming.h"

#include "motor_control.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static motor_control_source_t fake_source;
static motor_control_source_t fake_pending_source;
static motor_control_arm_result_t fake_arm_result;
static motor_control_disarm_result_t fake_disarm_result;
static uint32_t fake_arm_calls;
static uint32_t fake_disarm_calls;

motor_control_source_t motor_control_active_source(void)
{
    return fake_source;
}

motor_control_source_t motor_control_pending_source(void)
{
    return fake_pending_source;
}

motor_control_arm_result_t motor_control_arm(motor_control_source_t source)
{
    fake_arm_calls++;
    if (fake_arm_result == MOTOR_CONTROL_ARM_ACCEPTED) {
        fake_source = source;
    } else if (fake_arm_result == MOTOR_CONTROL_ARM_PENDING) {
        fake_pending_source = source;
    }
    return fake_arm_result;
}

motor_control_disarm_result_t motor_control_disarm(void)
{
    fake_disarm_calls++;
    if (fake_disarm_result == MOTOR_CONTROL_DISARM_ACCEPTED) {
        fake_source = MOTOR_CONTROL_SOURCE_NONE;
        fake_pending_source = MOTOR_CONTROL_SOURCE_NONE;
    }
    return fake_disarm_result;
}

static void reset_motor_fake(void)
{
    fake_source = MOTOR_CONTROL_SOURCE_NONE;
    fake_pending_source = MOTOR_CONTROL_SOURCE_NONE;
    fake_arm_result = MOTOR_CONTROL_ARM_ACCEPTED;
    fake_disarm_result = MOTOR_CONTROL_DISARM_ACCEPTED;
    fake_arm_calls = 0U;
    fake_disarm_calls = 0U;
}

static receiver_arming_t initialized_arming(void)
{
    receiver_arming_config_t config;
    receiver_arming_t arming;

    receiver_arming_default_config(&config);
    assert(receiver_arming_initialize(&arming, &config));
    return arming;
}

static receiver_control_state_t control(bool arm_high, float throttle)
{
    return (receiver_control_state_t){
        .snapshot = {
            .throttle = throttle,
            .arm_switch_high = arm_high,
            .valid = true,
        },
        .freshness = RECEIVER_FRESHNESS_FRESH,
    };
}

static receiver_failsafe_decision_t live_failsafe(void)
{
    return (receiver_failsafe_decision_t){
        .state = RECEIVER_FAILSAFE_LIVE,
        .action = RECEIVER_FAILSAFE_ACTION_LIVE,
    };
}

static void test_initialization_and_names(void)
{
    receiver_arming_config_t config;
    receiver_arming_t arming;

    receiver_arming_default_config(&config);
    assert(config.throttle_maximum == MOTOR_COMMAND_STOP_THRESHOLD);
    assert(receiver_arming_config_is_valid(&config));
    assert(!receiver_arming_initialize(NULL, &config));
    assert(!receiver_arming_initialize(&arming, NULL));
    config.throttle_maximum = NAN;
    assert(!receiver_arming_config_is_valid(&config));
    assert(!receiver_arming_initialize(&arming, &config));
    config.throttle_maximum = MOTOR_COMMAND_STOP_THRESHOLD + 0.0001F;
    assert(!receiver_arming_config_is_valid(&config));
    assert(receiver_arming_process(&arming, NULL, NULL) ==
           RECEIVER_ARMING_NOT_INITIALIZED);
    assert(strcmp(receiver_arming_result_name(RECEIVER_ARMING_READY),
                  "READY") == 0);
    assert(strcmp(receiver_arming_result_name(RECEIVER_ARMING_RESULT_COUNT),
                  "INVALID") == 0);
    assert(strcmp(receiver_arming_result_name(
                      (receiver_arming_result_t)-1),
                  "INVALID") == 0);
}

static void test_high_at_startup_cannot_arm(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(true, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(fake_arm_calls == 0U);
}

static void test_low_to_high_at_stop_threshold_arms_once(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input =
        control(false, MOTOR_COMMAND_STOP_THRESHOLD);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input.snapshot.arm_switch_high = true;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_ARM_ACCEPTED);
    assert(fake_source == MOTOR_CONTROL_SOURCE_RECEIVER);
    assert(fake_arm_calls == 1U);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_RECEIVER_ACTIVE);
    assert(fake_arm_calls == 1U);
}

static void test_high_throttle_requires_a_new_switch_cycle(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input = control(true, 0.1F);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_BLOCKED_THROTTLE);
    input.snapshot.throttle = 0.0F;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(fake_arm_calls == 0U);

    input.snapshot.arm_switch_high = false;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input.snapshot.arm_switch_high = true;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_ARM_ACCEPTED);
    assert(fake_arm_calls == 1U);
    assert(arming.arm_request_count == 2U);
    assert(arming.rejected_arm_count == 1U);
}

static void test_unusable_input_clears_low_qualification(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input.freshness = RECEIVER_FRESHNESS_STALE;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_INPUT_UNAVAILABLE);
    input = control(true, 0.0F);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(fake_arm_calls == 0U);

    input = control(false, 0.0F);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    failsafe.state = RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING;
    failsafe.action = RECEIVER_FAILSAFE_ACTION_STOP;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_INPUT_UNAVAILABLE);

    failsafe = live_failsafe();
    input = control(false, NAN);
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_INPUT_UNAVAILABLE);
}

static void test_receiver_low_disarms_and_can_rearm(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    fake_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_DISARM_ACCEPTED);
    assert(fake_disarm_calls == 1U);
    assert(fake_source == MOTOR_CONTROL_SOURCE_NONE);
    input.snapshot.arm_switch_high = true;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_ARM_ACCEPTED);
    assert(fake_arm_calls == 1U);
}

static void test_other_source_isolated_and_no_automatic_handoff(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    fake_source = MOTOR_CONTROL_SOURCE_USB_TEST;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_OTHER_SOURCE_ACTIVE);
    assert(fake_disarm_calls == 0U);

    input.snapshot.arm_switch_high = true;
    fake_source = MOTOR_CONTROL_SOURCE_NONE;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(fake_arm_calls == 0U);
}

static void test_rejected_motor_arm_consumes_the_edge(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    fake_arm_result = MOTOR_CONTROL_ARM_BLOCKED_HEALTH;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input.snapshot.arm_switch_high = true;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_ARM_REJECTED);
    fake_arm_result = MOTOR_CONTROL_ARM_ACCEPTED;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_WAITING_FOR_LOW);
    assert(fake_arm_calls == 1U);
}

static void test_rejected_disarm_is_reported(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    fake_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    fake_disarm_result = MOTOR_CONTROL_DISARM_TRANSITION_ERROR;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_DISARM_REJECTED);
    assert(fake_disarm_calls == 1U);
    assert(arming.disarm_request_count == 1U);
}

static void test_pending_arm_is_cancelled_when_input_becomes_unusable(void)
{
    receiver_arming_t arming = initialized_arming();
    receiver_control_state_t input = control(false, 0.0F);
    receiver_failsafe_decision_t failsafe = live_failsafe();

    fake_arm_result = MOTOR_CONTROL_ARM_PENDING;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_READY);
    input.snapshot.arm_switch_high = true;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_ARM_PENDING);
    assert(fake_pending_source == MOTOR_CONTROL_SOURCE_RECEIVER);

    input.freshness = RECEIVER_FRESHNESS_STALE;
    assert(receiver_arming_process(&arming, &input, &failsafe) ==
           RECEIVER_ARMING_INPUT_UNAVAILABLE);
    assert(fake_disarm_calls == 1U);
    assert(fake_pending_source == MOTOR_CONTROL_SOURCE_NONE);
}

int main(void)
{
    reset_motor_fake();
    test_initialization_and_names();
    reset_motor_fake();
    test_high_at_startup_cannot_arm();
    reset_motor_fake();
    test_low_to_high_at_stop_threshold_arms_once();
    reset_motor_fake();
    test_high_throttle_requires_a_new_switch_cycle();
    reset_motor_fake();
    test_unusable_input_clears_low_qualification();
    reset_motor_fake();
    test_receiver_low_disarms_and_can_rearm();
    reset_motor_fake();
    test_other_source_isolated_and_no_automatic_handoff();
    reset_motor_fake();
    test_rejected_motor_arm_consumes_the_edge();
    reset_motor_fake();
    test_rejected_disarm_is_reported();
    reset_motor_fake();
    test_pending_arm_is_cancelled_when_input_becomes_unusable();
    return 0;
}
