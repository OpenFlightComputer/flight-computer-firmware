#include "imu_sample.h"

#include <limits.h>
#include <stddef.h>

static unsigned int source_axis(imu_axis_selection_t selection)
{
    return (unsigned int)selection / 2U;
}

static bool selection_is_valid(imu_axis_selection_t selection)
{
    return (unsigned int)selection <= (unsigned int)IMU_AXIS_NEGATIVE_Z;
}

static int32_t select_value(imu_axis_selection_t selection,
                            int16_t x,
                            int16_t y,
                            int16_t z)
{
    const int16_t values[3] = {x, y, z};
    const int32_t value = values[source_axis(selection)];

    return (((unsigned int)selection & 1U) != 0U) ? -value : value;
}

bool imu_axis_mapping_is_valid(const imu_axis_mapping_t *mapping)
{
    if ((mapping == NULL) || !selection_is_valid(mapping->body_x) ||
        !selection_is_valid(mapping->body_y) ||
        !selection_is_valid(mapping->body_z)) {
        return false;
    }

    return source_axis(mapping->body_x) != source_axis(mapping->body_y) &&
           source_axis(mapping->body_x) != source_axis(mapping->body_z) &&
           source_axis(mapping->body_y) != source_axis(mapping->body_z);
}

bool imu_map_raw_sample(const imu_axis_mapping_t *mapping,
                        const imu_raw_sample_t *raw,
                        imu_sample_snapshot_t *mapped)
{
    if (!imu_axis_mapping_is_valid(mapping) || (raw == NULL) ||
        (mapped == NULL)) {
        return false;
    }

    *mapped = (imu_sample_snapshot_t){
        .acceleration_x = select_value(mapping->body_x,
                                       raw->acceleration_x,
                                       raw->acceleration_y,
                                       raw->acceleration_z),
        .acceleration_y = select_value(mapping->body_y,
                                       raw->acceleration_x,
                                       raw->acceleration_y,
                                       raw->acceleration_z),
        .acceleration_z = select_value(mapping->body_z,
                                       raw->acceleration_x,
                                       raw->acceleration_y,
                                       raw->acceleration_z),
        .gyroscope_x = select_value(mapping->body_x,
                                    raw->gyroscope_x,
                                    raw->gyroscope_y,
                                    raw->gyroscope_z),
        .gyroscope_y = select_value(mapping->body_y,
                                    raw->gyroscope_x,
                                    raw->gyroscope_y,
                                    raw->gyroscope_z),
        .gyroscope_z = select_value(mapping->body_z,
                                    raw->gyroscope_x,
                                    raw->gyroscope_y,
                                    raw->gyroscope_z),
        .valid = true,
    };
    return true;
}

bool imu_freshness_config_is_valid(const imu_freshness_config_t *config)
{
    return (config != NULL) && (config->fresh_through_us > 0U) &&
           (config->lost_after_us > config->fresh_through_us);
}

imu_freshness_t imu_freshness_evaluate(
    const imu_freshness_config_t *config,
    const imu_sample_snapshot_t *snapshot,
    uint64_t now_us,
    uint64_t *age_us)
{
    uint64_t age;

    if (age_us != NULL) {
        *age_us = UINT64_MAX;
    }
    if (!imu_freshness_config_is_valid(config) || (snapshot == NULL) ||
        !snapshot->valid) {
        return IMU_FRESHNESS_UNAVAILABLE;
    }
    if (now_us < snapshot->acquired_at_us) {
        return IMU_FRESHNESS_LOST;
    }

    age = now_us - snapshot->acquired_at_us;
    if (age_us != NULL) {
        *age_us = age;
    }
    if (age <= config->fresh_through_us) {
        return IMU_FRESHNESS_FRESH;
    }
    if (age <= config->lost_after_us) {
        return IMU_FRESHNESS_STALE;
    }
    return IMU_FRESHNESS_LOST;
}
