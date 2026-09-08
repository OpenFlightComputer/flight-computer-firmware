#ifndef OPENFLIGHTCOMPUTER_RECEIVER_NORMALIZATION_H
#define OPENFLIGHTCOMPUTER_RECEIVER_NORMALIZATION_H

#include "receiver_source.h"

#include <stdbool.h>
#include <stdint.h>

#define RECEIVER_RAW_CHANNEL_MAX 2047U

typedef struct {
    uint8_t channel;
    uint16_t minimum;
    uint16_t center;
    uint16_t maximum;
    bool reversed;
} receiver_axis_calibration_t;

typedef struct {
    uint8_t channel;
    uint16_t minimum;
    uint16_t maximum;
    bool reversed;
} receiver_throttle_calibration_t;

typedef struct {
    uint8_t channel;
    uint16_t high_minimum;
} receiver_switch_calibration_t;

typedef struct {
    receiver_axis_calibration_t roll;
    receiver_axis_calibration_t pitch;
    receiver_axis_calibration_t yaw;
    receiver_throttle_calibration_t throttle;
    receiver_switch_calibration_t arm;
} receiver_normalization_config_t;

typedef struct {
    float roll;
    float pitch;
    float yaw;
    float throttle;
    uint64_t received_at_us;
    uint32_t source_sequence;
    bool arm_switch_high;
    bool valid;
} receiver_control_snapshot_t;

typedef struct {
    receiver_normalization_config_t config;
    bool initialized;
} receiver_normalizer_t;

typedef enum {
    RECEIVER_NORMALIZATION_OK = 0,
    RECEIVER_NORMALIZATION_INVALID_ARGUMENT,
    RECEIVER_NORMALIZATION_INVALID_CONFIG,
    RECEIVER_NORMALIZATION_NOT_INITIALIZED,
} receiver_normalization_result_t;

void receiver_normalization_default_config(
    receiver_normalization_config_t *config);
bool receiver_normalization_config_is_valid(
    const receiver_normalization_config_t *config);
receiver_normalization_result_t receiver_normalizer_initialize(
    receiver_normalizer_t *normalizer,
    const receiver_normalization_config_t *config);
receiver_normalization_result_t receiver_normalize(
    const receiver_normalizer_t *normalizer,
    const receiver_channel_frame_t *frame,
    uint64_t received_at_us,
    uint32_t source_sequence,
    receiver_control_snapshot_t *snapshot);

#endif
