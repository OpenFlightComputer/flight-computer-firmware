#include "usb_imu_response.h"

#include "uint64_decimal.h"

#include <stdio.h>

static bool finish_response(int written, size_t capacity, size_t *length)
{
    if ((written < 0) || ((size_t)written >= capacity)) {
        *length = 0U;
        return false;
    }
    *length = (size_t)written;
    return true;
}

bool usb_imu_response_build(const usb_imu_diagnostics_t *diagnostics,
                            uint32_t request_id,
                            char *destination,
                            size_t capacity,
                            size_t *length)
{
    char sequence[UINT64_DECIMAL_BUFFER_CAPACITY];
    char age[UINT64_DECIMAL_BUFFER_CAPACITY];
    size_t formatted_length;
    int written;

    if ((diagnostics == NULL) || (destination == NULL) ||
        (capacity == 0U) || (length == NULL) ||
        ((unsigned int)diagnostics->state.freshness >
         (unsigned int)IMU_FRESHNESS_LOST) ||
        ((unsigned int)diagnostics->calibration_state >
         (unsigned int)GYRO_CALIBRATION_READY) ||
        (diagnostics->calibration_ready !=
         (diagnostics->calibration_state == GYRO_CALIBRATION_READY))) {
        return false;
    }

    if (!diagnostics->state.snapshot.valid) {
        written = snprintf(
            destination,
            capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"imu\",\"ok\":true,\"available\":false,"
            "\"sequence\":null,\"age_us\":null,\"freshness\":\"%s\","
            "\"acceleration_raw\":null,\"gyroscope_raw\":null,"
            "\"gyroscope_corrected_raw\":null,"
            "\"calibration\":{\"state\":\"%s\",\"progress_permille\":%lu,"
            "\"samples\":%lu,\"restarts\":%lu,\"bias_raw\":null},"
            "\"service\":{\"reads\":%lu,\"published\":%lu,"
            "\"source_errors\":%lu},\"task\":null,"
            "\"high_rate\":{\"budget_us\":%lu,"
            "\"utilization_permille\":%lu}}\n",
            (unsigned long)request_id,
            imu_freshness_name(diagnostics->state.freshness),
            gyro_calibration_state_name(diagnostics->calibration_state),
            (unsigned long)diagnostics->calibration_progress_permille,
            (unsigned long)diagnostics->calibration_sample_count,
            (unsigned long)diagnostics->calibration_restart_count,
            (unsigned long)diagnostics->service_statistics.read_count,
            (unsigned long)
                diagnostics->service_statistics.published_sample_count,
            (unsigned long)
                diagnostics->service_statistics.source_error_count,
            (unsigned long)diagnostics->high_rate_budget_us,
            (unsigned long)diagnostics->high_rate_utilization_permille);
        return finish_response(written, capacity, length);
    }

    if (!diagnostics->task_present ||
        !uint64_decimal_format(diagnostics->state.snapshot.sequence,
                               0U,
                               sequence,
                               sizeof(sequence),
                               &formatted_length) ||
        !uint64_decimal_format(diagnostics->state.age_us,
                               0U,
                               age,
                               sizeof(age),
                               &formatted_length)) {
        *length = 0U;
        return false;
    }

    if (diagnostics->calibration_ready) {
        written = snprintf(
            destination, capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"imu\",\"ok\":true,\"available\":true,"
            "\"sequence\":%s,\"age_us\":%s,\"freshness\":\"%s\","
            "\"acceleration_raw\":{\"x\":%ld,\"y\":%ld,\"z\":%ld},"
            "\"gyroscope_raw\":{\"x\":%ld,\"y\":%ld,\"z\":%ld},"
            "\"gyroscope_corrected_raw\":{\"x\":%ld,\"y\":%ld,"
            "\"z\":%ld},\"calibration\":{\"state\":\"%s\","
            "\"progress_permille\":%lu,\"samples\":%lu,"
            "\"restarts\":%lu,\"bias_raw\":{\"x\":%ld,\"y\":%ld,"
            "\"z\":%ld}},\"service\":{\"reads\":%lu,"
            "\"published\":%lu,\"source_errors\":%lu},"
            "\"task\":{\"executions\":%lu,\"last_execution_us\":%lu,"
            "\"maximum_execution_us\":%lu,\"overruns\":%lu,"
            "\"missed_releases\":%lu},\"high_rate\":{\"budget_us\":%lu,"
            "\"utilization_permille\":%lu}}\n",
            (unsigned long)request_id, sequence, age,
            imu_freshness_name(diagnostics->state.freshness),
            (long)diagnostics->state.snapshot.acceleration_x,
            (long)diagnostics->state.snapshot.acceleration_y,
            (long)diagnostics->state.snapshot.acceleration_z,
            (long)diagnostics->state.snapshot.gyroscope_x,
            (long)diagnostics->state.snapshot.gyroscope_y,
            (long)diagnostics->state.snapshot.gyroscope_z,
            (long)diagnostics->corrected_gyroscope[0],
            (long)diagnostics->corrected_gyroscope[1],
            (long)diagnostics->corrected_gyroscope[2],
            gyro_calibration_state_name(diagnostics->calibration_state),
            (unsigned long)diagnostics->calibration_progress_permille,
            (unsigned long)diagnostics->calibration_sample_count,
            (unsigned long)diagnostics->calibration_restart_count,
            (long)diagnostics->calibration_bias[0],
            (long)diagnostics->calibration_bias[1],
            (long)diagnostics->calibration_bias[2],
            (unsigned long)diagnostics->service_statistics.read_count,
            (unsigned long)diagnostics->service_statistics.published_sample_count,
            (unsigned long)diagnostics->service_statistics.source_error_count,
            (unsigned long)diagnostics->task_execution_count,
            (unsigned long)diagnostics->task_last_execution_us,
            (unsigned long)diagnostics->task_maximum_execution_us,
            (unsigned long)diagnostics->task_overrun_count,
            (unsigned long)diagnostics->task_missed_release_count,
            (unsigned long)diagnostics->high_rate_budget_us,
            (unsigned long)diagnostics->high_rate_utilization_permille);
    } else {
        written = snprintf(
            destination, capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"imu\",\"ok\":true,\"available\":true,"
            "\"sequence\":%s,\"age_us\":%s,\"freshness\":\"%s\","
            "\"acceleration_raw\":{\"x\":%ld,\"y\":%ld,\"z\":%ld},"
            "\"gyroscope_raw\":{\"x\":%ld,\"y\":%ld,\"z\":%ld},"
            "\"gyroscope_corrected_raw\":null,\"calibration\":{"
            "\"state\":\"%s\",\"progress_permille\":%lu,"
            "\"samples\":%lu,\"restarts\":%lu,\"bias_raw\":null},"
            "\"service\":{\"reads\":%lu,\"published\":%lu,"
            "\"source_errors\":%lu},\"task\":{\"executions\":%lu,"
            "\"last_execution_us\":%lu,\"maximum_execution_us\":%lu,"
            "\"overruns\":%lu,\"missed_releases\":%lu},"
            "\"high_rate\":{\"budget_us\":%lu,"
            "\"utilization_permille\":%lu}}\n",
            (unsigned long)request_id, sequence, age,
            imu_freshness_name(diagnostics->state.freshness),
            (long)diagnostics->state.snapshot.acceleration_x,
            (long)diagnostics->state.snapshot.acceleration_y,
            (long)diagnostics->state.snapshot.acceleration_z,
            (long)diagnostics->state.snapshot.gyroscope_x,
            (long)diagnostics->state.snapshot.gyroscope_y,
            (long)diagnostics->state.snapshot.gyroscope_z,
            gyro_calibration_state_name(diagnostics->calibration_state),
            (unsigned long)diagnostics->calibration_progress_permille,
            (unsigned long)diagnostics->calibration_sample_count,
            (unsigned long)diagnostics->calibration_restart_count,
            (unsigned long)diagnostics->service_statistics.read_count,
            (unsigned long)diagnostics->service_statistics.published_sample_count,
            (unsigned long)diagnostics->service_statistics.source_error_count,
            (unsigned long)diagnostics->task_execution_count,
            (unsigned long)diagnostics->task_last_execution_us,
            (unsigned long)diagnostics->task_maximum_execution_us,
            (unsigned long)diagnostics->task_overrun_count,
            (unsigned long)diagnostics->task_missed_release_count,
            (unsigned long)diagnostics->high_rate_budget_us,
            (unsigned long)diagnostics->high_rate_utilization_permille);
    }
    return finish_response(written, capacity, length);
}
