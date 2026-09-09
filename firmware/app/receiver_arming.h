#ifndef OPENFLIGHTCOMPUTER_RECEIVER_ARMING_H
#define OPENFLIGHTCOMPUTER_RECEIVER_ARMING_H

#include "receiver_failsafe.h"
#include "receiver_service.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float throttle_maximum;
} receiver_arming_config_t;

typedef enum {
    RECEIVER_ARMING_IDLE = 0,
    RECEIVER_ARMING_INPUT_UNAVAILABLE,
    RECEIVER_ARMING_OTHER_SOURCE_ACTIVE,
    RECEIVER_ARMING_WAITING_FOR_LOW,
    RECEIVER_ARMING_READY,
    RECEIVER_ARMING_RECEIVER_ACTIVE,
    RECEIVER_ARMING_BLOCKED_THROTTLE,
    RECEIVER_ARMING_ARM_PENDING,
    RECEIVER_ARMING_ARM_ACCEPTED,
    RECEIVER_ARMING_ARM_REJECTED,
    RECEIVER_ARMING_DISARM_ACCEPTED,
    RECEIVER_ARMING_DISARM_REJECTED,
    RECEIVER_ARMING_NOT_INITIALIZED,
    RECEIVER_ARMING_RESULT_COUNT,
} receiver_arming_result_t;

typedef struct {
    receiver_arming_config_t config;
    receiver_arming_result_t last_result;
    uint32_t arm_request_count;
    uint32_t rejected_arm_count;
    uint32_t disarm_request_count;
    bool arm_low_seen;
    bool previous_arm_switch_high;
    bool switch_observed;
    bool initialized;
} receiver_arming_t;

void receiver_arming_default_config(receiver_arming_config_t *config);
bool receiver_arming_config_is_valid(const receiver_arming_config_t *config);
bool receiver_arming_initialize(receiver_arming_t *arming,
                                const receiver_arming_config_t *config);
receiver_arming_result_t receiver_arming_process(
    receiver_arming_t *arming,
    const receiver_control_state_t *control,
    const receiver_failsafe_decision_t *failsafe);
const char *receiver_arming_result_name(receiver_arming_result_t result);

#endif
