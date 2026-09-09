#include "application_tasks.h"

#include "application_state.h"
#include "board_receiver.h"
#include "fault_catalog.h"
#include "flight_control.h"
#include "logging.h"
#include "motor_control_internal.h"
#include "time.h"
#include "usb_cdc_transport.h"

#include <stddef.h>
#include <string.h>

#define RECEIVER_TASK_PERIOD_US UINT32_C(1000)

static receiver_freshness_state_t logged_receiver_freshness =
    RECEIVER_FRESHNESS_UNAVAILABLE;
static receiver_failsafe_state_t logged_receiver_failsafe_state =
    RECEIVER_FAILSAFE_UNAVAILABLE;
static bool receiver_source_fault_reported;
static bool receiver_connection_fault_reported;

static void motor_control_task(void *context)
{
    (void)context;
    firmware_motor_control_sync_last_result =
        (uint32_t)motor_control_synchronize();
    firmware_motor_control_task_executions++;
}

static void receiver_task(void *context)
{
    receiver_service_t *service = context;
    receiver_control_state_t control = {0};
    board_receiver_statistics_t statistics = {0};
    receiver_service_result_t result;

    result = receiver_service_process_once(service);
    firmware_receiver_service_last_result = (uint32_t)result;
    firmware_receiver_task_executions++;

    if (board_receiver_statistics(&statistics)) {
        firmware_receiver_uart_bytes = statistics.uart_received_byte_count;
        firmware_receiver_valid_frames = statistics.valid_frame_count;
        firmware_receiver_crc_errors = statistics.crc_error_count;
        firmware_receiver_framing_errors = statistics.framing_error_count;
        firmware_receiver_dma_overruns = statistics.dma_overrun_count;
        firmware_receiver_dma_dropped_bytes =
            statistics.dma_dropped_byte_count;
    }

    if ((result == RECEIVER_SERVICE_SOURCE_ERROR) &&
        !receiver_source_fault_reported) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_RECEIVER_SOURCE,
                                          true,
                                          statistics.uart_error);
        receiver_source_fault_reported = true;
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "source error context=%lu",
                  (unsigned long)statistics.uart_error);
    }

    if (receiver_service_control_state(service, &control)) {
        firmware_receiver_freshness = (uint32_t)control.freshness;
        if (control.freshness != logged_receiver_freshness) {
            if (control.freshness == RECEIVER_FRESHNESS_FRESH) {
                LOG_INFO(LOG_MODULE_RECEIVER, "receiver data fresh");
            } else if (control.freshness == RECEIVER_FRESHNESS_STALE) {
                LOG_WARN(LOG_MODULE_RECEIVER, "receiver data stale");
            } else if (control.freshness == RECEIVER_FRESHNESS_LOST) {
                LOG_ERROR(LOG_MODULE_RECEIVER, "receiver data lost");
            }
            logged_receiver_freshness = control.freshness;
        }
    }
}

static void log_receiver_arming_result(receiver_arming_result_t result)
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

static void log_receiver_failsafe_transition(
    const receiver_failsafe_decision_t *decision)
{
    if (decision->state == logged_receiver_failsafe_state) {
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
    logged_receiver_failsafe_state = decision->state;
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
               receiver_connection_fault_reported) {
        if (fault_system_clear(&firmware_fault_system,
                               FAULT_ID_RECEIVER_CONNECTION_LOST) ==
            FAULT_CLEAR_OK) {
            receiver_connection_fault_reported = false;
        }
    }
}

static void flight_control_task(void *context)
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
        return;
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
    log_receiver_arming_result(
        (receiver_arming_result_t)firmware_receiver_arming_last_result);
    log_receiver_failsafe_transition(&decision);
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
        return;
    }
    firmware_flight_control_submit_last_result =
        (uint32_t)flight_control_process_receiver(
            &firmware_flight_configuration_service.active,
            &decision,
            now_us);
}

static bool receiver_inspection_read(void *context,
                                     receiver_inspection_t *inspection)
{
    receiver_service_t *service = context;
    receiver_snapshot_t raw_snapshot;
    receiver_control_state_t control_state;
    board_receiver_statistics_t statistics;
    const uint64_t now_us = time_us();

    if ((service == NULL) || (inspection == NULL)) {
        return false;
    }
    *inspection = (receiver_inspection_t){
        .freshness = RECEIVER_FRESHNESS_UNAVAILABLE,
        .failsafe_state = firmware_receiver_failsafe_decision.state,
        .failsafe_action = firmware_receiver_failsafe_decision.action,
        .stage_two_latched =
            firmware_receiver_failsafe_decision.stage_two_latched,
        .recovery_ready = firmware_receiver_failsafe_decision.recovery_ready,
    };

    if (receiver_service_latest(service, &raw_snapshot) &&
        receiver_service_control_state(service, &control_state) &&
        control_state.snapshot.valid &&
        (control_state.snapshot.source_sequence == raw_snapshot.sequence)) {
        memcpy(inspection->channels,
               raw_snapshot.frame.channels,
               sizeof(inspection->channels));
        inspection->sequence = raw_snapshot.sequence;
        inspection->age_us =
            now_us >= raw_snapshot.received_at_us
                ? now_us - raw_snapshot.received_at_us
                : UINT64_MAX;
        inspection->freshness = control_state.freshness;
        inspection->roll = control_state.snapshot.roll;
        inspection->pitch = control_state.snapshot.pitch;
        inspection->yaw = control_state.snapshot.yaw;
        inspection->throttle = control_state.snapshot.throttle;
        inspection->arm_switch_high =
            control_state.snapshot.arm_switch_high;
        inspection->available = true;
    }

    if (board_receiver_statistics(&statistics)) {
        inspection->link_statistics_present =
            statistics.link_statistics_present;
        inspection->uplink_rssi_dbm = statistics.uplink_rssi_dbm;
        inspection->uplink_link_quality_percent =
            statistics.uplink_link_quality_percent;
        inspection->uplink_snr_db = statistics.uplink_snr_db;
        inspection->uart_received_byte_count =
            statistics.uart_received_byte_count;
        inspection->valid_frame_count = statistics.valid_frame_count;
        inspection->crc_error_count = statistics.crc_error_count;
        inspection->framing_error_count = statistics.framing_error_count;
        inspection->dma_overrun_count = statistics.dma_overrun_count;
        inspection->dma_dropped_byte_count =
            statistics.dma_dropped_byte_count;
    }
    return true;
}

static const receiver_inspection_provider_t receiver_inspection_provider = {
    .read = receiver_inspection_read,
    .context = &firmware_receiver_service,
};

const receiver_inspection_provider_t *
application_receiver_inspection_provider(void)
{
    return &receiver_inspection_provider;
}

static void diagnostic_fast_task(void *context)
{
    (void)context;
    firmware_fast_task_executions++;
}

static void diagnostic_medium_task(void *context)
{
    (void)context;
    firmware_medium_task_executions++;
}

static void diagnostic_slow_task(void *context)
{
    (void)context;
    firmware_slow_task_executions++;
}

static void usb_service_task(void *context)
{
    (void)context;
    usb_cdc_transport_process();
    firmware_usb_command_last_result =
        (uint32_t)usb_command_processor_process_once(
            &firmware_usb_command_processor);
    if (firmware_usb_command_processor.last_transition_valid) {
        firmware_system_state_last_result =
            (uint32_t)firmware_usb_command_processor.last_transition_result;
    }
    firmware_logging_drain_last_result = (uint32_t)logging_drain_once();
    firmware_usb_service_task_executions++;
}

static const task_definition_t diagnostic_task_definitions[] = {
    {.name = "diagnostic-fast",
     .period_us = 1000U,
     .priority = TASK_PRIORITY_HIGH,
     .callback = diagnostic_fast_task},
    {.name = "diagnostic-medium",
     .period_us = 10000U,
     .priority = TASK_PRIORITY_NORMAL,
     .callback = diagnostic_medium_task},
    {.name = "diagnostic-slow",
     .period_us = 100000U,
     .priority = TASK_PRIORITY_LOW,
     .callback = diagnostic_slow_task},
};

static const task_definition_t usb_service_task_definition = {
    .name = "usb-service",
    .period_us = 1000U,
    .priority = TASK_PRIORITY_BACKGROUND,
    .callback = usb_service_task,
};

static const task_definition_t motor_control_task_definition = {
    .name = "motor-control",
    .period_us = MOTOR_CONTROL_FRAME_PERIOD_US,
    .priority = TASK_PRIORITY_HIGHEST,
    .callback = motor_control_task,
};

static const task_definition_t receiver_task_definition = {
    .name = "receiver-service",
    .period_us = RECEIVER_TASK_PERIOD_US,
    .priority = TASK_PRIORITY_HIGH,
    .callback = receiver_task,
    .context = &firmware_receiver_service,
};

static const task_definition_t flight_control_task_definition = {
    .name = "flight-control",
    .period_us = RECEIVER_TASK_PERIOD_US,
    .priority = TASK_PRIORITY_HIGH,
    .callback = flight_control_task,
    .context = &firmware_receiver_service,
};

static task_registration_result_t register_task(
    const task_definition_t *definition)
{
    return task_registry_register(&firmware_task_registry, definition);
}

task_registration_result_t application_tasks_register(
    bool usb_service_available,
    bool receiver_service_available)
{
    size_t index;
    task_registration_result_t result;

    task_registry_initialize(&firmware_task_registry);
    result = register_task(&motor_control_task_definition);
    if (result != TASK_REGISTRATION_OK) {
        return result;
    }
    if (receiver_service_available) {
        result = register_task(&receiver_task_definition);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
        result = register_task(&flight_control_task_definition);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    for (index = 0U;
         index < (sizeof(diagnostic_task_definitions) /
                  sizeof(diagnostic_task_definitions[0]));
         index++) {
        result = register_task(&diagnostic_task_definitions[index]);
        if (result != TASK_REGISTRATION_OK) {
            return result;
        }
    }
    if (usb_service_available) {
        return register_task(&usb_service_task_definition);
    }
    return TASK_REGISTRATION_OK;
}
