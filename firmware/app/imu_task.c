#include "application_task_definitions.h"

#include "application_state.h"
#include "fault_catalog.h"
#include "imu_service.h"
#include "logging.h"

#define IMU_TASK_PERIOD_US UINT32_C(1000)

static bool communication_fault_reported;
static bool communication_problem_logged;
static imu_freshness_t logged_freshness = IMU_FRESHNESS_UNAVAILABLE;

static bool freshness_is_unusable(imu_freshness_t freshness)
{
    return (freshness == IMU_FRESHNESS_STALE) ||
           (freshness == IMU_FRESHNESS_LOST);
}

static void update_communication_fault(const imu_service_t *service,
                                       imu_freshness_t freshness)
{
    if (freshness_is_unusable(freshness) &&
        !communication_fault_reported) {
        const fault_report_result_t report_result =
            fault_system_report(&firmware_fault_system,
                                FAULT_ID_IMU_COMMUNICATION,
                                true,
                                service->statistics.source_error_count);

        firmware_fault_last_result = (uint32_t)report_result;
        communication_fault_reported =
            (report_result == FAULT_REPORT_RECORDED) ||
            (report_result == FAULT_REPORT_UPDATED);
    } else if ((freshness == IMU_FRESHNESS_FRESH) &&
               communication_fault_reported &&
               (fault_system_clear(&firmware_fault_system,
                                   FAULT_ID_IMU_COMMUNICATION) ==
                FAULT_CLEAR_OK)) {
        communication_fault_reported = false;
    }
}

static void log_communication_state(const imu_service_t *service,
                                    imu_freshness_t freshness)
{
    if (freshness_is_unusable(freshness) &&
        !communication_problem_logged) {
        LOG_ERROR(LOG_MODULE_IMU,
                  "BMI270 runtime data unavailable errors=%lu",
                  (unsigned long)service->statistics.source_error_count);
        communication_problem_logged = true;
    } else if ((freshness == IMU_FRESHNESS_FRESH) &&
               communication_problem_logged) {
        LOG_INFO(LOG_MODULE_IMU, "BMI270 runtime data recovered");
        communication_problem_logged = false;
    }
}

static void log_freshness_transition(imu_freshness_t freshness)
{
    if (freshness == logged_freshness) {
        return;
    }
    if (freshness == IMU_FRESHNESS_FRESH) {
        LOG_INFO(LOG_MODULE_IMU, "IMU data fresh");
    } else if (freshness == IMU_FRESHNESS_STALE) {
        LOG_WARN(LOG_MODULE_IMU, "IMU data stale");
    } else if (freshness == IMU_FRESHNESS_LOST) {
        LOG_ERROR(LOG_MODULE_IMU, "IMU data lost");
    }
    logged_freshness = freshness;
}

static void run_imu_task(void *context)
{
    imu_service_t *service = context;
    imu_service_state_t state;
    const imu_service_result_t result = imu_service_process_once(service);

    firmware_imu_service_last_result = (uint32_t)result;
    firmware_imu_task_executions++;
    firmware_imu_source_error_count = service->statistics.source_error_count;

    if (!imu_service_state(service, &state)) {
        return;
    }
    firmware_imu_freshness = (uint32_t)state.freshness;
    firmware_imu_sample_age_us = state.age_us;
    firmware_imu_sample_sequence = state.snapshot.sequence;
    firmware_imu_body_acceleration_x = state.snapshot.acceleration_x;
    firmware_imu_body_acceleration_y = state.snapshot.acceleration_y;
    firmware_imu_body_acceleration_z = state.snapshot.acceleration_z;
    firmware_imu_body_gyroscope_x = state.snapshot.gyroscope_x;
    firmware_imu_body_gyroscope_y = state.snapshot.gyroscope_y;
    firmware_imu_body_gyroscope_z = state.snapshot.gyroscope_z;

    update_communication_fault(service, state.freshness);
    log_communication_state(service, state.freshness);
    log_freshness_transition(state.freshness);
}

const task_definition_t *imu_task_definition(void)
{
    static const task_definition_t definition = {
        .name = "imu-service",
        .period_us = IMU_TASK_PERIOD_US,
        .priority = TASK_PRIORITY_HIGH,
        .callback = run_imu_task,
        .context = &firmware_imu_service,
    };

    return &definition;
}
