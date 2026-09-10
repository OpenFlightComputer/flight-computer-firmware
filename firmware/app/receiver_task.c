#include "application_task_definitions.h"

#include "application_state.h"
#include "board_receiver.h"
#include "fault_catalog.h"
#include "logging.h"
#include "time.h"

#include <stddef.h>
#include <string.h>

#define RECEIVER_TASK_PERIOD_US UINT32_C(1000)

static receiver_freshness_state_t logged_freshness =
    RECEIVER_FRESHNESS_UNAVAILABLE;
static bool source_fault_reported;

static task_callback_result_t run_receiver_task(void *context)
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

    if ((result == RECEIVER_SERVICE_SOURCE_ERROR) && !source_fault_reported) {
        firmware_fault_last_result =
            (uint32_t)fault_system_report(&firmware_fault_system,
                                          FAULT_ID_RECEIVER_SOURCE,
                                          true,
                                          statistics.uart_error);
        source_fault_reported = true;
        LOG_ERROR(LOG_MODULE_RECEIVER,
                  "source error context=%lu",
                  (unsigned long)statistics.uart_error);
    }

    if (receiver_service_control_state(service, &control)) {
        firmware_receiver_freshness = (uint32_t)control.freshness;
        if (control.freshness != logged_freshness) {
            if (control.freshness == RECEIVER_FRESHNESS_FRESH) {
                LOG_INFO(LOG_MODULE_RECEIVER, "receiver data fresh");
            } else if (control.freshness == RECEIVER_FRESHNESS_STALE) {
                LOG_WARN(LOG_MODULE_RECEIVER, "receiver data stale");
            } else if (control.freshness == RECEIVER_FRESHNESS_LOST) {
                LOG_ERROR(LOG_MODULE_RECEIVER, "receiver data lost");
            }
            logged_freshness = control.freshness;
        }
    }
    return TASK_CALLBACK_CONTINUE;
}

static bool read_receiver_inspection(void *context,
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

const task_definition_t *receiver_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "receiver-service",
        .period_us = RECEIVER_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_HIGH,
        .callback = run_receiver_task,
        .context = &firmware_receiver_service,
    };

    return &definition;
}

const receiver_inspection_provider_t *receiver_task_inspection_provider(void)
{
    static const receiver_inspection_provider_t provider = {
        .read = read_receiver_inspection,
        .context = &firmware_receiver_service,
    };

    return &provider;
}
