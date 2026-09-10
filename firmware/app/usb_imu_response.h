#ifndef OPENFLIGHTCOMPUTER_USB_IMU_RESPONSE_H
#define OPENFLIGHTCOMPUTER_USB_IMU_RESPONSE_H

#include "imu_service.h"
#include "gyro_calibration.h"
#include "imu_processing_pipeline.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    imu_service_state_t state;
    imu_service_statistics_t service_statistics;
    uint32_t task_execution_count;
    uint32_t task_last_execution_us;
    uint32_t task_maximum_execution_us;
    uint32_t task_overrun_count;
    uint32_t task_missed_release_count;
    uint32_t high_rate_budget_us;
    uint32_t high_rate_utilization_permille;
    bool task_present;
    gyro_calibration_state_t calibration_state;
    int32_t calibration_bias[3];
    int32_t corrected_gyroscope[3];
    uint32_t calibration_sample_count;
    uint32_t calibration_restart_count;
    uint32_t calibration_progress_permille;
    bool calibration_ready;
    attitude_snapshot_t attitude;
    imu_processing_statistics_t processing_statistics;
} usb_imu_diagnostics_t;

bool usb_imu_response_build(const usb_imu_diagnostics_t *diagnostics,
                            uint32_t request_id,
                            char *destination,
                            size_t capacity,
                            size_t *length);

#endif
