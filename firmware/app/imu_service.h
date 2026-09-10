#ifndef OPENFLIGHTCOMPUTER_IMU_SERVICE_H
#define OPENFLIGHTCOMPUTER_IMU_SERVICE_H

#include "imu_sample.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IMU_SOURCE_SAMPLE_AVAILABLE = 0,
    IMU_SOURCE_ERROR,
} imu_source_result_t;

typedef imu_source_result_t (*imu_source_read_t)(void *context,
                                                imu_raw_sample_t *sample);
typedef uint64_t (*imu_service_clock_t)(void);

typedef struct {
    imu_source_read_t read;
    void *context;
} imu_source_t;

typedef struct {
    uint32_t read_count;
    uint32_t published_sample_count;
    uint32_t source_error_count;
} imu_service_statistics_t;

typedef enum {
    IMU_SERVICE_SAMPLE_PUBLISHED = 0,
    IMU_SERVICE_SOURCE_ERROR,
    IMU_SERVICE_NOT_INITIALIZED,
} imu_service_result_t;

typedef struct {
    imu_sample_snapshot_t snapshot;
    imu_freshness_t freshness;
    uint64_t age_us;
} imu_service_state_t;

typedef struct {
    imu_source_t source;
    imu_service_clock_t clock;
    imu_axis_mapping_t axis_mapping;
    imu_freshness_config_t freshness_config;
    imu_sample_snapshot_t latest;
    uint64_t initialized_at_us;
    imu_service_statistics_t statistics;
    imu_service_result_t last_result;
    bool initialized;
} imu_service_t;

bool imu_service_initialize(imu_service_t *service,
                            const imu_source_t *source,
                            imu_service_clock_t clock,
                            const imu_axis_mapping_t *axis_mapping,
                            const imu_freshness_config_t *freshness_config);
imu_service_result_t imu_service_process_once(imu_service_t *service);
bool imu_service_state(const imu_service_t *service,
                       imu_service_state_t *state);

#endif
