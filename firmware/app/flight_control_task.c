#include "application_task_definitions.h"

#include "application_state.h"
#include "flight_diagnostics.h"
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

static const receiver_control_snapshot_t *latest_receiver_control(
    receiver_service_t *service,
    receiver_control_state_t *control)
{
    *control = (receiver_control_state_t){0};
    if (receiver_service_control_state(service, control) &&
        control->snapshot.valid) {
        return &control->snapshot;
    }
    return NULL;
}

static void publish_failsafe_decision(
    const receiver_failsafe_decision_t *decision)
{
    firmware_receiver_failsafe_decision = *decision;
    firmware_receiver_failsafe_state = (uint32_t)decision->state;
    firmware_receiver_failsafe_action = (uint32_t)decision->action;
    firmware_receiver_link_age_us = decision->link_age_us;
    firmware_receiver_failsafe_transitions = decision->transition_count;
    firmware_receiver_stage_two_latched = decision->stage_two_latched;
    firmware_receiver_recovery_ready = decision->recovery_ready;
}

static void process_receiver_status(
    const receiver_control_state_t *control,
    const receiver_failsafe_decision_t *decision)
{
    const receiver_arming_result_t arming_result = receiver_arming_process(
        &firmware_receiver_arming, control, decision);

    firmware_receiver_arming_last_result = (uint32_t)arming_result;
    log_arming_result(arming_result);
    log_failsafe_transition(decision);
    update_receiver_connection_fault(decision);
}

static flight_control_result_t recover_receiver_control(void)
{
    const flight_control_result_t result = flight_control_recover_receiver(
        &firmware_receiver_failsafe);

    if (result == FLIGHT_CONTROL_RECOVERED) {
        LOG_INFO(LOG_MODULE_RECEIVER,
                 "receiver failsafe recovered to disarmed");
    }
    return result;
}

static flight_control_stabilization_t read_stabilization_inputs(
    attitude_snapshot_t *attitude,
    imu_service_state_t *imu_state,
    vehicle_state_t *vehicle_state)
{
    bool attitude_is_current;

    *attitude = (attitude_snapshot_t){0};
    *imu_state = (imu_service_state_t){0};
    (void)imu_service_state(&firmware_imu_service, imu_state);
    (void)imu_processing_pipeline_latest(
        &firmware_imu_processing_pipeline, attitude);
    attitude_is_current = attitude->valid && imu_state->snapshot.valid &&
                          (attitude->source_sequence ==
                           imu_state->snapshot.sequence) &&
                           (attitude->acquired_at_us ==
                           imu_state->snapshot.acquired_at_us);

    *vehicle_state = (vehicle_state_t){0};
    if (attitude_is_current) {
        vehicle_state->attitude_degrees[RATE_CONTROLLER_AXIS_ROLL] =
            attitude->roll_degrees;
        vehicle_state->attitude_degrees[RATE_CONTROLLER_AXIS_PITCH] =
            attitude->pitch_degrees;
        vehicle_state->angular_rate_dps[RATE_CONTROLLER_AXIS_ROLL] =
            attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_ROLL];
        vehicle_state->angular_rate_dps[RATE_CONTROLLER_AXIS_PITCH] =
            attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_PITCH];
        vehicle_state->angular_rate_dps[RATE_CONTROLLER_AXIS_YAW] =
            attitude->filtered_gyroscope_dps[RATE_CONTROLLER_AXIS_YAW];
        vehicle_state->acquired_at_us = attitude->acquired_at_us;
        vehicle_state->source_sequence = attitude->source_sequence;
        vehicle_state->valid = true;
    }

    return (flight_control_stabilization_t){
        .core = &firmware_flight_control_core,
        .profile = &firmware_flight_configuration_service
                        .prepared_control_profile,
        .vehicle_state = attitude_is_current ? vehicle_state : NULL,
        .desired_rates = &firmware_flight_control_desired_rates,
        .rate_output = &firmware_rate_controller_output,
        .rate_result = &firmware_rate_controller_result,
    };
}

static void capture_manual_behavior_observation(
    flight_diagnostic_control_t *diagnostic)
{
    diagnostic->setpoint = firmware_manual_easy_behavior.last_setpoint;
    diagnostic->effective_roll_degrees = firmware_manual_easy_behavior
                                             .takeoff_leveling
                                             .effective_roll_degrees;
    diagnostic->effective_pitch_degrees = firmware_manual_easy_behavior
                                              .takeoff_leveling
                                              .effective_pitch_degrees;
    diagnostic->takeoff_leveling_state =
        firmware_manual_easy_behavior.takeoff_leveling.state;
}

static flight_control_result_t execute_manual_easy_behavior(
    const receiver_failsafe_decision_t *decision,
    imu_freshness_t imu_freshness,
    flight_control_stabilization_t *stabilization,
    flight_diagnostic_control_t *diagnostic,
    uint64_t now_us)
{
    control_objective_t objective;
    flight_behavior_result_t behavior_result;

    if (motor_control_active_source() != MOTOR_CONTROL_SOURCE_RECEIVER) {
        manual_easy_behavior_reset(
            &firmware_manual_easy_behavior,
            &firmware_flight_configuration_service.active.easy_mode);
        flight_control_reset(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    behavior_result = manual_easy_behavior_update(
        &firmware_manual_easy_behavior,
        &firmware_flight_configuration_service.prepared_control,
        &firmware_flight_configuration_service.active.easy_mode,
        decision,
        stabilization->vehicle_state,
        imu_freshness,
        now_us,
        &objective);
    capture_manual_behavior_observation(diagnostic);
    if (behavior_result == FLIGHT_BEHAVIOR_INACTIVE) {
        flight_control_reset(stabilization);
        return FLIGHT_CONTROL_IDLE;
    }
    if (behavior_result == FLIGHT_BEHAVIOR_STOP_REQUESTED) {
        return flight_control_enter_failsafe(
            stabilization, FLIGHT_CONTROL_FAILSAFE_ENTERED);
    }
    if (behavior_result == FLIGHT_BEHAVIOR_WAITING_FOR_STATE) {
        return FLIGHT_CONTROL_WAITING_FOR_IMU;
    }
    if (behavior_result == FLIGHT_BEHAVIOR_INVALID_STATE) {
        return flight_control_enter_failsafe(
            stabilization, FLIGHT_CONTROL_IMU_FAILSAFE_ENTERED);
    }
    if (behavior_result != FLIGHT_BEHAVIOR_OBJECTIVE_READY) {
        return flight_control_enter_failsafe(
            stabilization, FLIGHT_CONTROL_CONTROL_FAILSAFE_ENTERED);
    }
    {
        const flight_control_result_t result =
            flight_control_execute_objective(
                MOTOR_CONTROL_SOURCE_RECEIVER,
                &objective,
                stabilization,
                &diagnostic->control,
                now_us);

        if (diagnostic->control.rate_result ==
            RATE_CONTROLLER_RESULT_SEEDED) {
            diagnostic->setpoint.throttle = 0.0F;
        }
        return result;
    }
}

static task_callback_result_t run_flight_control_task(void *context)
{
    receiver_service_t *service = context;
    receiver_control_state_t control;
    receiver_failsafe_decision_t decision;
    const receiver_control_snapshot_t *control_snapshot;
    attitude_snapshot_t attitude = {0};
    vehicle_state_t vehicle_state;
    imu_service_state_t imu_state;
    flight_control_stabilization_t stabilization;
    flight_diagnostic_control_t output = {0};
    flight_control_result_t result;
    const uint64_t now_us = time_us();

    firmware_flight_control_task_executions++;
    control_snapshot = latest_receiver_control(service, &control);
    if (!receiver_failsafe_update(&firmware_receiver_failsafe,
                                  control_snapshot,
                                  now_us,
                                  &decision)) {
        return TASK_CALLBACK_CONTINUE;
    }

    publish_failsafe_decision(&decision);
    process_receiver_status(&control, &decision);

    if (decision.recovery_ready) {
        result = recover_receiver_control();
    } else {
        stabilization = read_stabilization_inputs(
            &attitude, &imu_state, &vehicle_state);
        result = execute_manual_easy_behavior(
            &decision,
            imu_state.freshness,
            &stabilization,
            &output,
            now_us);
    }
    firmware_flight_control_submit_last_result = (uint32_t)result;
    flight_diagnostics_capture(
        now_us, &decision, &attitude, &output, result);
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
