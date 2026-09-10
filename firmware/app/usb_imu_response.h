#ifndef OPENFLIGHTCOMPUTER_USB_IMU_RESPONSE_H
#define OPENFLIGHTCOMPUTER_USB_IMU_RESPONSE_H

#include "imu_service.h"

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
} usb_imu_diagnostics_t;

bool usb_imu_response_build(const usb_imu_diagnostics_t *diagnostics,
                            uint32_t request_id,
                            char *destination,
                            size_t capacity,
                            size_t *length);

#endif
