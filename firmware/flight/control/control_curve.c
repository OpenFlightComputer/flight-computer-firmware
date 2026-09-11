#include "control_curve.h"

#include <math.h>
#include <stddef.h>

static bool close_to(float left, float right)
{
    return fabsf(left - right) <= 0.000001F;
}

bool control_curve_config_is_valid(const control_curve_config_t *config)
{
    size_t index;

    if ((config == NULL) ||
        (config->type != CONTROL_CURVE_TYPE_CONTROL_POINTS) ||
        (config->interpolation != CONTROL_CURVE_INTERPOLATION_LINEAR) ||
        (config->point_count < 2U) ||
        (config->point_count > CONTROL_CURVE_MAXIMUM_POINTS) ||
        !close_to(config->points[0].input, 0.0F) ||
        !close_to(config->points[0].output, 0.0F) ||
        !close_to(config->points[config->point_count - 1U].input, 1.0F) ||
        !close_to(config->points[config->point_count - 1U].output, 1.0F)) {
        return false;
    }
    for (index = 0U; index < config->point_count; index++) {
        const control_curve_point_t *point = &config->points[index];

        if (!isfinite(point->input) || !isfinite(point->output) ||
            (point->input < 0.0F) || (point->input > 1.0F) ||
            (point->output < 0.0F) || (point->output > 1.0F)) {
            return false;
        }
        if ((index > 0U) &&
            ((point->input <= config->points[index - 1U].input) ||
             (point->output < config->points[index - 1U].output))) {
            return false;
        }
    }
    return true;
}

bool control_curve_prepare(const control_curve_config_t *config,
                           prepared_control_curve_t *prepared)
{
    size_t index;

    if ((prepared == NULL) || !control_curve_config_is_valid(config)) {
        return false;
    }
    *prepared = (prepared_control_curve_t){
        .type = config->type,
        .interpolation = config->interpolation,
        .segment_count = (uint8_t)(config->point_count - 1U),
    };
    for (index = 0U; index < prepared->segment_count; index++) {
        const control_curve_point_t *lower = &config->points[index];
        const control_curve_point_t *upper = &config->points[index + 1U];
        const float slope =
            (upper->output - lower->output) /
            (upper->input - lower->input);

        prepared->segments[index] = (control_curve_segment_t){
            .upper_input = upper->input,
            .coefficient = {
                lower->output - (slope * lower->input),
                slope,
                0.0F,
                0.0F,
            },
        };
    }
    prepared->initialized = true;
    return true;
}

bool control_curve_apply(const prepared_control_curve_t *prepared,
                         float input,
                         float *output)
{
    const control_curve_segment_t *segment;
    float value;
    size_t index;

    if ((prepared == NULL) || !prepared->initialized || (output == NULL) ||
        !isfinite(input) || (input < 0.0F) || (input > 1.0F) ||
        (prepared->type != CONTROL_CURVE_TYPE_CONTROL_POINTS) ||
        (prepared->interpolation != CONTROL_CURVE_INTERPOLATION_LINEAR) ||
        (prepared->segment_count == 0U) ||
        (prepared->segment_count > CONTROL_CURVE_MAXIMUM_SEGMENTS)) {
        return false;
    }
    segment = &prepared->segments[prepared->segment_count - 1U];
    for (index = 0U; index < prepared->segment_count; index++) {
        if (input <= prepared->segments[index].upper_input) {
            segment = &prepared->segments[index];
            break;
        }
    }
    value = ((segment->coefficient[3] * input + segment->coefficient[2]) *
                 input +
             segment->coefficient[1]) *
                input +
            segment->coefficient[0];
    if (!isfinite(value)) {
        return false;
    }
    if (value < 0.0F) {
        value = 0.0F;
    } else if (value > 1.0F) {
        value = 1.0F;
    }
    *output = value;
    return true;
}

const char *control_curve_type_name(control_curve_type_t type)
{
    return type == CONTROL_CURVE_TYPE_CONTROL_POINTS ? "CONTROL_POINTS"
                                                     : "INVALID";
}

const char *control_curve_interpolation_name(
    control_curve_interpolation_t interpolation)
{
    return interpolation == CONTROL_CURVE_INTERPOLATION_LINEAR ? "LINEAR"
                                                               : "INVALID";
}
