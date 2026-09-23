#ifndef OPENFLIGHTCOMPUTER_VEHICLE_STATE_H
#define OPENFLIGHTCOMPUTER_VEHICLE_STATE_H

#include "flight_control_core.h"
#include "imu_processing_pipeline.h"
#include "imu_sample.h"

#include <stdbool.h>

bool vehicle_state_from_imu(
    const attitude_snapshot_t *attitude,
    const imu_sample_snapshot_t *source_sample,
    vehicle_state_t *state);

#endif
