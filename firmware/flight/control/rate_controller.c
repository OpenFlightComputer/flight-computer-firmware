#include "rate_controller.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

bool rate_controller_config_is_valid(const rate_controller_config_t *config)
{
    size_t axis;

    if ((config == NULL) || (config->type != RATE_CONTROLLER_TYPE_PID) ||
        (config->maximum_gap_us == 0U) ||
        (config->maximum_gap_us > UINT32_C(1000000))) {
        return false;
    }
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!rate_pid_config_is_valid(&config->axis[axis])) {
            return false;
        }
    }
    return true;
}

bool rate_controller_initialize(rate_controller_t *controller,
                                const rate_controller_config_t *config)
{
    size_t axis;

    if ((controller == NULL) || !rate_controller_config_is_valid(config)) {
        return false;
    }
    *controller = (rate_controller_t){
        .config = *config,
        .initialized = true,
    };
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!rate_pid_initialize(&controller->axis[axis],
                                 &config->axis[axis])) {
            *controller = (rate_controller_t){0};
            return false;
        }
    }
    return true;
}

void rate_controller_reset(rate_controller_t *controller)
{
    size_t axis;

    if ((controller == NULL) || !controller->initialized) {
        return;
    }
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        rate_pid_reset(&controller->axis[axis]);
    }
    controller->previous_acquired_at_us = 0U;
    controller->previous_source_sequence = 0U;
    controller->sample_seeded = false;
}

static bool inputs_are_finite(
    const float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    const float measured_rate_dps[RATE_CONTROLLER_AXIS_COUNT])
{
    size_t axis;

    if ((desired_rate_dps == NULL) || (measured_rate_dps == NULL)) {
        return false;
    }
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!isfinite(desired_rate_dps[axis]) ||
            !isfinite(measured_rate_dps[axis])) {
            return false;
        }
    }
    return true;
}

static bool seed(rate_controller_t *controller,
                 const float measured_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
                 uint64_t acquired_at_us,
                 uint64_t source_sequence)
{
    size_t axis;

    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!rate_pid_seed_measurement(&controller->axis[axis],
                                       measured_rate_dps[axis])) {
            return false;
        }
    }
    controller->previous_acquired_at_us = acquired_at_us;
    controller->previous_source_sequence = source_sequence;
    controller->sample_seeded = true;
    return true;
}

rate_controller_result_t rate_controller_process(
    rate_controller_t *controller,
    const float desired_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    const float measured_rate_dps[RATE_CONTROLLER_AXIS_COUNT],
    uint64_t acquired_at_us,
    uint64_t source_sequence,
    bool control_enabled,
    rate_controller_output_t *output)
{
    uint64_t gap_us;
    size_t axis;

    if (output != NULL) {
        *output = (rate_controller_output_t){0};
    }
    if ((controller == NULL) || (output == NULL) ||
        !controller->initialized) {
        return RATE_CONTROLLER_RESULT_NOT_INITIALIZED;
    }
    if (!control_enabled) {
        rate_controller_reset(controller);
        return RATE_CONTROLLER_RESULT_DISABLED;
    }
    if (!inputs_are_finite(desired_rate_dps, measured_rate_dps)) {
        rate_controller_reset(controller);
        return RATE_CONTROLLER_RESULT_INVALID_INPUT;
    }
    if (!controller->sample_seeded) {
        if (!seed(controller, measured_rate_dps, acquired_at_us,
                  source_sequence)) {
            rate_controller_reset(controller);
            return RATE_CONTROLLER_RESULT_INVALID_INPUT;
        }
        return RATE_CONTROLLER_RESULT_SEEDED;
    }
    if ((source_sequence == controller->previous_source_sequence) &&
        (acquired_at_us == controller->previous_acquired_at_us)) {
        return RATE_CONTROLLER_RESULT_NO_NEW_SAMPLE;
    }
    if ((source_sequence <= controller->previous_source_sequence) ||
        (acquired_at_us <= controller->previous_acquired_at_us)) {
        rate_controller_reset(controller);
        return RATE_CONTROLLER_RESULT_CONTINUITY_LOST;
    }
    gap_us = acquired_at_us - controller->previous_acquired_at_us;
    if ((gap_us > controller->config.maximum_gap_us) ||
        (gap_us > UINT32_MAX)) {
        rate_controller_reset(controller);
        (void)seed(controller, measured_rate_dps, acquired_at_us,
                   source_sequence);
        return RATE_CONTROLLER_RESULT_CONTINUITY_LOST;
    }
    for (axis = 0U; axis < RATE_CONTROLLER_AXIS_COUNT; axis++) {
        if (!rate_pid_update(&controller->axis[axis],
                             desired_rate_dps[axis],
                             measured_rate_dps[axis],
                             (float)gap_us / 1000000.0F,
                             &output->axis[axis])) {
            rate_controller_reset(controller);
            return RATE_CONTROLLER_RESULT_INVALID_INPUT;
        }
    }
    controller->previous_acquired_at_us = acquired_at_us;
    controller->previous_source_sequence = source_sequence;
    output->acquired_at_us = acquired_at_us;
    output->source_sequence = source_sequence;
    output->dt_us = (uint32_t)gap_us;
    output->valid = true;
    return RATE_CONTROLLER_RESULT_UPDATED;
}

const char *rate_controller_type_name(rate_controller_type_t type)
{
    return type == RATE_CONTROLLER_TYPE_PID ? "PID" : "UNKNOWN";
}
