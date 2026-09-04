#ifndef OPENFLIGHTCOMPUTER_MOTOR_SAFETY_POLICY_H
#define OPENFLIGHTCOMPUTER_MOTOR_SAFETY_POLICY_H

#include "fault.h"
#include "health.h"

#include <stdbool.h>

bool motor_health_allows_output(health_state_t health);
bool motor_fault_state_allows_arm(const fault_system_t *fault_system);

#endif
