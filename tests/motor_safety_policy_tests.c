#include "motor_safety_policy.h"

#include <assert.h>
#include <stddef.h>

static void health_policy_is_explicit(void)
{
    assert(motor_health_allows_output(HEALTH_STATE_OK));
    assert(motor_health_allows_output(HEALTH_STATE_WARNING));
    assert(motor_health_allows_output(HEALTH_STATE_DEGRADED));
    assert(!motor_health_allows_output(HEALTH_STATE_UNKNOWN));
    assert(!motor_health_allows_output(HEALTH_STATE_CRITICAL));
    assert(!motor_health_allows_output(HEALTH_STATE_COUNT));
    assert(!motor_health_allows_output((health_state_t)99));
}

static void arm_policy_uses_the_complete_health_projection(void)
{
    static const fault_definition_t definitions[] = {
        {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
        {2U, FAULT_SEVERITY_FAULT, FAULT_SOURCE_APPLICATION},
        {3U, FAULT_SEVERITY_CRITICAL, FAULT_SOURCE_APPLICATION},
    };
    system_state_machine_t state_machine;
    fault_system_t fault_system;

    system_state_machine_initialize(&state_machine);
    assert(fault_system_initialize(&fault_system,
                                   &state_machine,
                                   definitions,
                                   3U) == FAULT_INIT_OK);
    assert(motor_fault_state_allows_arm(&fault_system));

    assert(fault_system_report(&fault_system, 1U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(motor_fault_state_allows_arm(&fault_system));
    assert(fault_system_report(&fault_system, 2U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(motor_fault_state_allows_arm(&fault_system));

    fault_system.dropped_record_count = 1U;
    assert(!motor_fault_state_allows_arm(&fault_system));
    fault_system.dropped_record_count = 0U;

    assert(fault_system_report(&fault_system, 3U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    assert(!motor_fault_state_allows_arm(&fault_system));
    assert(!motor_fault_state_allows_arm(NULL));
}

int main(void)
{
    health_policy_is_explicit();
    arm_policy_uses_the_complete_health_projection();
    return 0;
}
