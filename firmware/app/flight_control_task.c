#include "application_task_definitions.h"

#include "application_state.h"
#include "fault_catalog.h"
#include "flight_control.h"
#include "logging.h"
#include "time.h"

#define FLIGHT_CONTROL_TASK_PERIOD_US UINT32_C(1000)

static receiver_failsafe_state_t logged_failsafe_state =
    RECEIVER_FAILSAFE_UNAVAILABLE;
static bool receiver_connection_fault_reported;

static void log_arming_result(receiver_arming_result_t result)
{
    if (result == RECEIVER_ARMING_ARM_ACCEPTED) {
        LOG_INFO(LOG_MODULE_RECEIVER, "receiver arm accepted");
    } else if (result == RECEIVER_ARMING_ARM_PENDING) {
        LOG_INFO(LOG_MODULE_RECEIVER,
                 "receiver arm pending motor direction configuration");
    } else if (result == RECEIVER_ARMING_DISARM_ACCEPTED) {
        LOG_INFO(LOG_MODULE_RECEIVER, "receiver disarm accepted");
    } else if (result == RECEIVER_ARMING_BLOCKED_THROTTLE) {
        LOG_WARN(LOG_MODULE_RECEIVER,
                 "receiver arm blocked: throttle not at stop");
    } else if ((result == RECEIVER_ARMING_ARM_REJECTED) ||
               (result == RECEIVER_ARMING_DISARM_REJECTED)) {
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "receiver lifecycle request rejected result=%s",
                  receiver_arming_result_name(result));
    }
}

static void log_failsafe_transition(
    const receiver_failsafe_decision_t *decision)
{
    if (decision->state == logged_failsafe_state) {
        return;
    }
    if ((decision->state == RECEIVER_FAILSAFE_LIVE) ||
        (decision->state == RECEIVER_FAILSAFE_STALE_HOLD)) {
        LOG_INFO(LOG_MODULE_RECEIVER,
                 "failsafe state=%s action=%s",
                 receiver_failsafe_state_name(decision->state),
                 receiver_failsafe_action_name(decision->action));
    } else if ((decision->state == RECEIVER_FAILSAFE_LOSS_HOLD) ||
               (decision->state == RECEIVER_FAILSAFE_STAGE_ONE)) {
        LOG_WARN(LOG_MODULE_RECEIVER,
                 "failsafe state=%s action=%s",
                 receiver_failsafe_state_name(decision->state),
                 receiver_failsafe_action_name(decision->action));
    } else {
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "failsafe state=%s action=%s",
                  receiver_failsafe_state_name(decision->state),
                  receiver_failsafe_action_name(decision->action));
    }
    logged_failsafe_state = decision->state;
}

static void update_receiver_connection_fault(
    const receiver_failsafe_decision_t *decision)
{
    if (decision->receiver_loss_detected &&
        !receiver_connection_fault_reported) {
        const fault_report_result_t report_result =
            fault_system_report(&firmware_fault_system,
                                FAULT_ID_RECEIVER_CONNECTION_LOST,
                                true,
                                (uint32_t)decision->state);

        firmware_fault_last_result = (uint32_t)report_result;
        receiver_connection_fault_reported =
            (report_result == FAULT_REPORT_RECORDED) ||
            (report_result == FAULT_REPORT_UPDATED);
    } else if (!decision->receiver_loss_detected &&
               receiver_connection_fault_reported &&
               (fault_system_clear(&firmware_fault_system,
                                   FAULT_ID_RECEIVER_CONNECTION_LOST) ==
                FAULT_CLEAR_OK)) {
        receiver_connection_fault_reported = false;
    }
}

static task_callback_result_t run_flight_control_task(void *context)
{
    receiver_service_t *service = context;
    receiver_control_state_t control = {0};
    receiver_failsafe_decision_t decision;
    const receiver_control_snapshot_t *control_snapshot = NULL;
    const uint64_t now_us = time_us();

    firmware_flight_control_task_executions++;
    if (receiver_service_control_state(service, &control) &&
        control.snapshot.valid) {
        control_snapshot = &control.snapshot;
    }
    if (!receiver_failsafe_update(&firmware_receiver_failsafe,
                                  control_snapshot,
                                  now_us,
                                  &decision)) {
        return TASK_CALLBACK_CONTINUE;
    }

    firmware_receiver_failsafe_decision = decision;
    firmware_receiver_failsafe_state = (uint32_t)decision.state;
    firmware_receiver_failsafe_action = (uint32_t)decision.action;
    firmware_receiver_link_age_us = decision.link_age_us;
    firmware_receiver_failsafe_transitions = decision.transition_count;
    firmware_receiver_stage_two_latched = decision.stage_two_latched;
    firmware_receiver_recovery_ready = decision.recovery_ready;

    firmware_receiver_arming_last_result =
        (uint32_t)receiver_arming_process(&firmware_receiver_arming,
                                          &control,
                                          &decision);
    log_arming_result(
        (receiver_arming_result_t)firmware_receiver_arming_last_result);
    log_failsafe_transition(&decision);
    update_receiver_connection_fault(&decision);

    if (decision.recovery_ready) {
        firmware_flight_control_submit_last_result =
            (uint32_t)flight_control_recover_receiver(
                &firmware_receiver_failsafe);
        if (firmware_flight_control_submit_last_result ==
            (uint32_t)FLIGHT_CONTROL_RECOVERED) {
            LOG_INFO(LOG_MODULE_RECEIVER,
                     "receiver failsafe recovered to disarmed");
        }
        return TASK_CALLBACK_CONTINUE;
    }
    firmware_flight_control_submit_last_result =
        (uint32_t)flight_control_process_receiver(
            &firmware_flight_configuration_service.active,
            &decision,
            now_us);
    return TASK_CALLBACK_CONTINUE;
}

const task_definition_t *flight_control_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "flight-control",
        .period_us = FLIGHT_CONTROL_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_HIGH,
        .callback = run_flight_control_task,
        .context = &firmware_receiver_service,
    };

    return &definition;
}
