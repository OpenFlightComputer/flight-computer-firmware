#include "dshot_motor_backend.h"

#include "board.h"
#include "dshot_encoder.h"

#include <stddef.h>
#include <stdint.h>

#define DSHOT_NORMALIZED_THROTTLE_SPAN \
    ((float)(DSHOT_THROTTLE_MAX - DSHOT_THROTTLE_MIN))

_Static_assert(MOTOR_COMMAND_MOTOR_COUNT == DSHOT_OUTPUT_COUNT,
               "Every physical motor requires one DShot table column");

static uint16_t normalized_throttle_to_dshot(float throttle)
{
    if (throttle == 0.0f) {
        return DSHOT_VALUE_STOP;
    }

    return (uint16_t)(DSHOT_THROTTLE_MIN +
                      (uint16_t)((throttle *
                                  DSHOT_NORMALIZED_THROTTLE_SPAN) +
                                 0.5f));
}

static bool build_stop_buffer(dshot_motor_backend_t *backend)
{
    uint16_t stop_frames[MOTOR_COMMAND_MOTOR_COUNT];
    size_t physical_output;

    for (physical_output = 0U;
         physical_output < MOTOR_COMMAND_MOTOR_COUNT;
         physical_output++) {
        if (dshot_encode_throttle(DSHOT_VALUE_STOP,
                                  false,
                                  &stop_frames[physical_output]) !=
            DSHOT_ENCODE_OK) {
            return false;
        }
    }

    return dshot_timing_build_dma_buffer(&backend->timing,
                                         stop_frames,
                                         backend->stop_buffer) ==
           DSHOT_TIMING_OK;
}

static motor_output_backend_init_result_t initialize_backend(void *context)
{
    dshot_motor_backend_t *backend = context;

    if ((backend == NULL) || !backend->prepared || backend->initialized ||
        (dshot_timing_profile_create(
             DSHOT_RATE_300,
             board_motor_output_timer_clock_frequency_hz(),
             &backend->timing) != DSHOT_TIMING_OK) ||
        !build_stop_buffer(backend)) {
        return MOTOR_OUTPUT_BACKEND_INIT_ERROR;
    }

    if (board_motor_output_initialize(backend->timing.bit_period_ticks) !=
        BOARD_MOTOR_OUTPUT_INIT_OK) {
        return MOTOR_OUTPUT_BACKEND_INIT_ERROR;
    }

    backend->initialized = true;
    return MOTOR_OUTPUT_BACKEND_INIT_OK;
}

static motor_output_backend_submit_result_t submit_command(
    const motor_command_t *command,
    void *context)
{
    dshot_motor_backend_t *backend = context;
    uint16_t frames_by_physical_output[MOTOR_COMMAND_MOTOR_COUNT];
    dshot_dma_buffer_t physical_compare_table;
    board_motor_output_status_t output_status;
    size_t physical_output;

    if ((backend == NULL) || !backend->initialized || (command == NULL)) {
        return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    }

    output_status = board_motor_output_status();
    if (output_status == BOARD_MOTOR_OUTPUT_STATUS_ACTIVE) {
        return MOTOR_OUTPUT_BACKEND_SUBMIT_BUSY;
    }
    if (output_status != BOARD_MOTOR_OUTPUT_STATUS_IDLE) {
        return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    }

    for (physical_output = 0U;
         physical_output < MOTOR_COMMAND_MOTOR_COUNT;
         physical_output++) {
        const uint16_t throttle = normalized_throttle_to_dshot(
            command->throttle[physical_output]);

        if (dshot_encode_throttle(throttle,
                                  false,
                                  &frames_by_physical_output[physical_output]) !=
            DSHOT_ENCODE_OK) {
            return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
        }
    }

    if (dshot_timing_build_dma_buffer(&backend->timing,
                                      frames_by_physical_output,
                                      physical_compare_table) !=
        DSHOT_TIMING_OK) {
        return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    }

    switch (board_motor_output_submit(&physical_compare_table[0][0],
                                      DSHOT_DMA_SLOT_COUNT *
                                          DSHOT_OUTPUT_COUNT)) {
    case BOARD_MOTOR_OUTPUT_SUBMIT_ACCEPTED:
        return MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
    case BOARD_MOTOR_OUTPUT_SUBMIT_BUSY:
        return MOTOR_OUTPUT_BACKEND_SUBMIT_BUSY;
    case BOARD_MOTOR_OUTPUT_SUBMIT_ERROR:
        return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
    }

    return MOTOR_OUTPUT_BACKEND_SUBMIT_ERROR;
}

static motor_output_backend_stop_result_t force_stop(void *context)
{
    dshot_motor_backend_t *backend = context;

    if ((backend == NULL) || !backend->initialized) {
        return MOTOR_OUTPUT_BACKEND_STOP_ERROR;
    }

    return board_motor_output_force_stop(
               &backend->stop_buffer[0][0],
               DSHOT_DMA_SLOT_COUNT * DSHOT_OUTPUT_COUNT) ==
               BOARD_MOTOR_OUTPUT_STOP_ACCEPTED
           ? MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED
           : MOTOR_OUTPUT_BACKEND_STOP_ERROR;
}

static motor_output_backend_status_t backend_status(void *context)
{
    const dshot_motor_backend_t *backend = context;

    if ((backend == NULL) || !backend->initialized) {
        return MOTOR_OUTPUT_BACKEND_STATUS_ERROR;
    }

    switch (board_motor_output_status()) {
    case BOARD_MOTOR_OUTPUT_STATUS_IDLE:
        return MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
    case BOARD_MOTOR_OUTPUT_STATUS_ACTIVE:
        return MOTOR_OUTPUT_BACKEND_STATUS_BUSY;
    case BOARD_MOTOR_OUTPUT_STATUS_UNINITIALIZED:
    case BOARD_MOTOR_OUTPUT_STATUS_ERROR:
        return MOTOR_OUTPUT_BACKEND_STATUS_ERROR;
    }

    return MOTOR_OUTPUT_BACKEND_STATUS_ERROR;
}

bool dshot_motor_backend_prepare(dshot_motor_backend_t *backend,
                                 motor_output_backend_t *output_backend)
{
    if ((backend == NULL) || (output_backend == NULL)) {
        return false;
    }

    *backend = (dshot_motor_backend_t){
        .prepared = true,
    };
    *output_backend = (motor_output_backend_t){
        .initialize = initialize_backend,
        .submit = submit_command,
        .force_stop = force_stop,
        .status = backend_status,
        .context = backend,
    };
    return true;
}
