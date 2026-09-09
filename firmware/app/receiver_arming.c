#include "receiver_arming.h"

#include "motor_control.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

static void saturating_increment(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        (*value)++;
    }
}

static receiver_arming_result_t set_result(
    receiver_arming_t *arming,
    receiver_arming_result_t result)
{
    arming->last_result = result;
    return result;
}

static void clear_arm_qualification(receiver_arming_t *arming)
{
    arming->arm_low_seen = false;
    arming->previous_arm_switch_high = false;
    arming->switch_observed = false;
}

static bool input_is_usable(
    const receiver_control_state_t *control,
    const receiver_failsafe_decision_t *failsafe)
{
    return (control != NULL) && control->snapshot.valid &&
           isfinite(control->snapshot.throttle) &&
           (control->snapshot.throttle >= 0.0F) &&
           (control->snapshot.throttle <= 1.0F) &&
           (control->freshness == RECEIVER_FRESHNESS_FRESH) &&
           (failsafe != NULL) &&
           (failsafe->state == RECEIVER_FAILSAFE_LIVE) &&
           (failsafe->action == RECEIVER_FAILSAFE_ACTION_LIVE);
}

void receiver_arming_default_config(receiver_arming_config_t *config)
{
    if (config != NULL) {
        *config = (receiver_arming_config_t){
            .throttle_maximum = MOTOR_COMMAND_STOP_THRESHOLD,
        };
    }
}

bool receiver_arming_config_is_valid(const receiver_arming_config_t *config)
{
    return (config != NULL) && isfinite(config->throttle_maximum) &&
           (config->throttle_maximum >= 0.0F) &&
           (config->throttle_maximum <= MOTOR_COMMAND_STOP_THRESHOLD);
}

bool receiver_arming_initialize(receiver_arming_t *arming,
                                const receiver_arming_config_t *config)
{
    if (arming == NULL) {
        return false;
    }

    *arming = (receiver_arming_t){0};
    if (!receiver_arming_config_is_valid(config)) {
        return false;
    }

    arming->config = *config;
    arming->last_result = RECEIVER_ARMING_IDLE;
    arming->initialized = true;
    return true;
}

receiver_arming_result_t receiver_arming_process(
    receiver_arming_t *arming,
    const receiver_control_state_t *control,
    const receiver_failsafe_decision_t *failsafe)
{
    motor_control_source_t source;
    bool switch_high;
    bool rising_edge;

    if ((arming == NULL) || !arming->initialized) {
        return RECEIVER_ARMING_NOT_INITIALIZED;
    }

    if (!input_is_usable(control, failsafe)) {
        clear_arm_qualification(arming);
        return set_result(arming, RECEIVER_ARMING_INPUT_UNAVAILABLE);
    }

    source = motor_control_active_source();
    if ((source != MOTOR_CONTROL_SOURCE_NONE) &&
        (source != MOTOR_CONTROL_SOURCE_RECEIVER)) {
        clear_arm_qualification(arming);
        return set_result(arming, RECEIVER_ARMING_OTHER_SOURCE_ACTIVE);
    }

    switch_high = control->snapshot.arm_switch_high;
    if (source == MOTOR_CONTROL_SOURCE_RECEIVER) {
        arming->switch_observed = true;
        arming->previous_arm_switch_high = switch_high;
        if (switch_high) {
            return set_result(arming, RECEIVER_ARMING_RECEIVER_ACTIVE);
        }

        saturating_increment(&arming->disarm_request_count);
        if (motor_control_disarm() == MOTOR_CONTROL_DISARM_ACCEPTED) {
            arming->arm_low_seen = true;
            return set_result(arming, RECEIVER_ARMING_DISARM_ACCEPTED);
        }
        return set_result(arming, RECEIVER_ARMING_DISARM_REJECTED);
    }

    if (!switch_high) {
        arming->arm_low_seen = true;
        arming->previous_arm_switch_high = false;
        arming->switch_observed = true;
        return set_result(arming, RECEIVER_ARMING_READY);
    }

    rising_edge = arming->switch_observed &&
                  !arming->previous_arm_switch_high;
    arming->previous_arm_switch_high = true;
    arming->switch_observed = true;
    if (!arming->arm_low_seen || !rising_edge) {
        return set_result(arming, RECEIVER_ARMING_WAITING_FOR_LOW);
    }

    arming->arm_low_seen = false;
    saturating_increment(&arming->arm_request_count);
    if (control->snapshot.throttle > arming->config.throttle_maximum) {
        saturating_increment(&arming->rejected_arm_count);
        return set_result(arming, RECEIVER_ARMING_BLOCKED_THROTTLE);
    }

    if (motor_control_arm(MOTOR_CONTROL_SOURCE_RECEIVER) ==
        MOTOR_CONTROL_ARM_ACCEPTED) {
        return set_result(arming, RECEIVER_ARMING_ARM_ACCEPTED);
    }

    saturating_increment(&arming->rejected_arm_count);
    return set_result(arming, RECEIVER_ARMING_ARM_REJECTED);
}

const char *receiver_arming_result_name(receiver_arming_result_t result)
{
    static const char *const names[] = {
        "IDLE",
        "INPUT_UNAVAILABLE",
        "OTHER_SOURCE_ACTIVE",
        "WAITING_FOR_LOW",
        "READY",
        "RECEIVER_ACTIVE",
        "BLOCKED_THROTTLE",
        "ARM_ACCEPTED",
        "ARM_REJECTED",
        "DISARM_ACCEPTED",
        "DISARM_REJECTED",
        "NOT_INITIALIZED",
    };

    if ((unsigned int)result >=
        (unsigned int)RECEIVER_ARMING_RESULT_COUNT) {
        return "INVALID";
    }
    return names[result];
}
