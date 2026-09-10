#ifndef OPENFLIGHTCOMPUTER_IMU_SAMPLE_H
#define OPENFLIGHTCOMPUTER_IMU_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IMU_AXIS_POSITIVE_X = 0,
    IMU_AXIS_NEGATIVE_X,
    IMU_AXIS_POSITIVE_Y,
    IMU_AXIS_NEGATIVE_Y,
    IMU_AXIS_POSITIVE_Z,
    IMU_AXIS_NEGATIVE_Z,
} imu_axis_selection_t;

typedef struct {
    imu_axis_selection_t body_x;
    imu_axis_selection_t body_y;
    imu_axis_selection_t body_z;
} imu_axis_mapping_t;

typedef struct {
    int16_t acceleration_x;
    int16_t acceleration_y;
    int16_t acceleration_z;
    int16_t gyroscope_x;
    int16_t gyroscope_y;
    int16_t gyroscope_z;
} imu_raw_sample_t;

typedef struct {
    int32_t acceleration_x;
    int32_t acceleration_y;
    int32_t acceleration_z;
    int32_t gyroscope_x;
    int32_t gyroscope_y;
    int32_t gyroscope_z;
    uint64_t acquired_at_us;
    uint64_t sequence;
    bool valid;
} imu_sample_snapshot_t;

typedef enum {
    IMU_FRESHNESS_UNAVAILABLE = 0,
    IMU_FRESHNESS_FRESH,
    IMU_FRESHNESS_STALE,
    IMU_FRESHNESS_LOST,
} imu_freshness_t;

typedef struct {
    uint32_t fresh_through_us;
    uint32_t lost_after_us;
} imu_freshness_config_t;

bool imu_axis_mapping_is_valid(const imu_axis_mapping_t *mapping);
bool imu_map_raw_sample(const imu_axis_mapping_t *mapping,
                        const imu_raw_sample_t *raw,
                        imu_sample_snapshot_t *mapped);
bool imu_freshness_config_is_valid(const imu_freshness_config_t *config);
imu_freshness_t imu_freshness_evaluate(
    const imu_freshness_config_t *config,
    const imu_sample_snapshot_t *snapshot,
    uint64_t now_us,
    uint64_t *age_us);

#endif
