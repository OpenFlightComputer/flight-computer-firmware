#include "motor_safety_policy.h"

bool motor_health_allows_output(health_state_t health)
{
    return (health == HEALTH_STATE_OK) ||
           (health == HEALTH_STATE_WARNING) ||
           (health == HEALTH_STATE_DEGRADED);
}

bool motor_fault_state_allows_arm(const fault_system_t *fault_system)
{
    health_summary_t summary;

    return (health_evaluate(fault_system, &summary) == HEALTH_EVALUATE_OK) &&
           motor_health_allows_output(summary.state);
}
