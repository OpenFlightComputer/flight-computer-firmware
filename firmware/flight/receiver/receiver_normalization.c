#include "receiver_normalization.h"

#include <stddef.h>

#define RECEIVER_DEFAULT_ROLL_MINIMUM 174U
#define RECEIVER_DEFAULT_ROLL_CENTER 992U
#define RECEIVER_DEFAULT_ROLL_MAXIMUM 1805U
#define RECEIVER_DEFAULT_PITCH_MINIMUM 175U
#define RECEIVER_DEFAULT_PITCH_CENTER 992U
#define RECEIVER_DEFAULT_PITCH_MAXIMUM 1811U
#define RECEIVER_DEFAULT_THROTTLE_MINIMUM 174U
#define RECEIVER_DEFAULT_THROTTLE_MAXIMUM 1785U
#define RECEIVER_DEFAULT_YAW_MINIMUM 355U
#define RECEIVER_DEFAULT_YAW_CENTER 997U
#define RECEIVER_DEFAULT_YAW_MAXIMUM 1713U
#define RECEIVER_DEFAULT_ARM_HIGH_MINIMUM 1500U

static bool axis_is_valid(const receiver_axis_calibration_t *axis)
{
    return (axis->channel < RECEIVER_CHANNEL_COUNT) &&
           (axis->minimum < axis->center) &&
           (axis->center < axis->maximum) &&
           (axis->maximum <= RECEIVER_RAW_CHANNEL_MAX);
}

static bool throttle_is_valid(
    const receiver_throttle_calibration_t *throttle)
{
    return (throttle->channel < RECEIVER_CHANNEL_COUNT) &&
           (throttle->minimum < throttle->maximum) &&
           (throttle->maximum <= RECEIVER_RAW_CHANNEL_MAX);
}

static bool channels_are_unique(
    const receiver_normalization_config_t *config)
{
    const uint8_t channels[] = {
        config->roll.channel,
        config->pitch.channel,
        config->yaw.channel,
        config->throttle.channel,
        config->arm.channel,
    };
    size_t first;
    size_t second;

    for (first = 0U; first < sizeof(channels) / sizeof(channels[0]); first++) {
        for (second = first + 1U;
             second < sizeof(channels) / sizeof(channels[0]);
             second++) {
            if (channels[first] == channels[second]) {
                return false;
            }
        }
    }
    return true;
}

static float clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float normalize_axis(uint16_t raw,
                            const receiver_axis_calibration_t *calibration,
                            float negative_scale,
                            float positive_scale)
{
    float normalized;

    if (raw >= calibration->center) {
        normalized =
            (float)((int32_t)raw - (int32_t)calibration->center) *
            positive_scale;
    } else {
        normalized =
            (float)((int32_t)raw - (int32_t)calibration->center) *
            negative_scale;
    }
    normalized = clamp(normalized, -1.0f, 1.0f);
    return calibration->reversed ? -normalized : normalized;
}

static float normalize_throttle(
    uint16_t raw,
    const receiver_throttle_calibration_t *calibration,
    float scale)
{
    float normalized =
        (float)((int32_t)raw - (int32_t)calibration->minimum) * scale;

    normalized = clamp(normalized, 0.0f, 1.0f);
    return calibration->reversed ? 1.0f - normalized : normalized;
}

void receiver_normalization_default_config(
    receiver_normalization_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (receiver_normalization_config_t){
        .roll = {
            .channel = 0U,
            .minimum = RECEIVER_DEFAULT_ROLL_MINIMUM,
            .center = RECEIVER_DEFAULT_ROLL_CENTER,
            .maximum = RECEIVER_DEFAULT_ROLL_MAXIMUM,
        },
        .pitch = {
            .channel = 1U,
            .minimum = RECEIVER_DEFAULT_PITCH_MINIMUM,
            .center = RECEIVER_DEFAULT_PITCH_CENTER,
            .maximum = RECEIVER_DEFAULT_PITCH_MAXIMUM,
        },
        .yaw = {
            .channel = 3U,
            .minimum = RECEIVER_DEFAULT_YAW_MINIMUM,
            .center = RECEIVER_DEFAULT_YAW_CENTER,
            .maximum = RECEIVER_DEFAULT_YAW_MAXIMUM,
        },
        .throttle = {
            .channel = 2U,
            .minimum = RECEIVER_DEFAULT_THROTTLE_MINIMUM,
            .maximum = RECEIVER_DEFAULT_THROTTLE_MAXIMUM,
        },
        .arm = {
            .channel = 4U,
            .high_minimum = RECEIVER_DEFAULT_ARM_HIGH_MINIMUM,
        },
    };
}

bool receiver_normalization_config_is_valid(
    const receiver_normalization_config_t *config)
{
    return (config != NULL) && axis_is_valid(&config->roll) &&
           axis_is_valid(&config->pitch) && axis_is_valid(&config->yaw) &&
           throttle_is_valid(&config->throttle) &&
           (config->arm.channel < RECEIVER_CHANNEL_COUNT) &&
           (config->arm.high_minimum > 0U) &&
           (config->arm.high_minimum <= RECEIVER_RAW_CHANNEL_MAX) &&
           channels_are_unique(config);
}

receiver_normalization_result_t receiver_normalizer_initialize(
    receiver_normalizer_t *normalizer,
    const receiver_normalization_config_t *config)
{
    if (normalizer == NULL) {
        return RECEIVER_NORMALIZATION_INVALID_ARGUMENT;
    }
    if (!receiver_normalization_config_is_valid(config)) {
        *normalizer = (receiver_normalizer_t){0};
        return RECEIVER_NORMALIZATION_INVALID_CONFIG;
    }

    *normalizer = (receiver_normalizer_t){
        .config = *config,
        .axis_negative_scale = {
            1.0F / (float)(config->roll.center - config->roll.minimum),
            1.0F / (float)(config->pitch.center - config->pitch.minimum),
            1.0F / (float)(config->yaw.center - config->yaw.minimum),
        },
        .axis_positive_scale = {
            1.0F / (float)(config->roll.maximum - config->roll.center),
            1.0F / (float)(config->pitch.maximum - config->pitch.center),
            1.0F / (float)(config->yaw.maximum - config->yaw.center),
        },
        .throttle_scale =
            1.0F / (float)(config->throttle.maximum -
                           config->throttle.minimum),
        .initialized = true,
    };
    return RECEIVER_NORMALIZATION_OK;
}

receiver_normalization_result_t receiver_normalize(
    const receiver_normalizer_t *normalizer,
    const receiver_channel_frame_t *frame,
    uint64_t received_at_us,
    uint32_t source_sequence,
    receiver_control_snapshot_t *snapshot)
{
    const receiver_normalization_config_t *config;

    if ((normalizer == NULL) || (frame == NULL) || (snapshot == NULL)) {
        return RECEIVER_NORMALIZATION_INVALID_ARGUMENT;
    }
    if (!normalizer->initialized) {
        return RECEIVER_NORMALIZATION_NOT_INITIALIZED;
    }

    config = &normalizer->config;
    *snapshot = (receiver_control_snapshot_t){
        .roll = normalize_axis(frame->channels[config->roll.channel],
                               &config->roll,
                               normalizer->axis_negative_scale[0],
                               normalizer->axis_positive_scale[0]),
        .pitch = normalize_axis(frame->channels[config->pitch.channel],
                                &config->pitch,
                                normalizer->axis_negative_scale[1],
                                normalizer->axis_positive_scale[1]),
        .yaw = normalize_axis(frame->channels[config->yaw.channel],
                              &config->yaw,
                              normalizer->axis_negative_scale[2],
                              normalizer->axis_positive_scale[2]),
        .throttle = normalize_throttle(
            frame->channels[config->throttle.channel], &config->throttle,
            normalizer->throttle_scale),
        .received_at_us = received_at_us,
        .source_sequence = source_sequence,
        .arm_switch_high =
            frame->channels[config->arm.channel] >= config->arm.high_minimum,
        .valid = true,
    };
    return RECEIVER_NORMALIZATION_OK;
}
