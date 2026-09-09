#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONTROL_H

#include "flight_configuration.h"
#include "receiver_failsafe.h"

#include <stdint.h>

typedef enum {
    FLIGHT_CONTROL_IDLE = 0,
    FLIGHT_CONTROL_SUBMITTED,
    FLIGHT_CONTROL_FAILSAFE_ENTERED,
    FLIGHT_CONTROL_RECOVERED,
    FLIGHT_CONTROL_MIX_ERROR,
    FLIGHT_CONTROL_SUBMIT_ERROR,
    FLIGHT_CONTROL_FAILSAFE_ERROR,
    FLIGHT_CONTROL_RECOVERY_ERROR,
} flight_control_result_t;

flight_control_result_t flight_control_process_receiver(
    const flight_configuration_t *configuration,
    const receiver_failsafe_decision_t *decision,
    uint64_t now_us);
flight_control_result_t flight_control_recover_receiver(
    receiver_failsafe_t *failsafe);

#endif
