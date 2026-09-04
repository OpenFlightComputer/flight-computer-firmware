#ifndef OPENFLIGHTCOMPUTER_DSHOT_TIMING_H
#define OPENFLIGHTCOMPUTER_DSHOT_TIMING_H

#include <stdint.h>

#include "dshot_encoder.h"

#define DSHOT_TIMER_LANE_COUNT 4U
#define DSHOT_TRAILING_LOW_SLOT_COUNT 2U
#define DSHOT_DMA_SLOT_COUNT \
    (DSHOT_FRAME_BIT_COUNT + DSHOT_TRAILING_LOW_SLOT_COUNT)

typedef enum {
    DSHOT_RATE_300 = 300000,
} dshot_rate_t;

typedef struct {
    dshot_rate_t rate;
    uint32_t timer_clock_hz;
    uint16_t bit_period_ticks;
    uint16_t zero_high_ticks;
    uint16_t one_high_ticks;
} dshot_timing_profile_t;

typedef uint16_t
    dshot_dma_buffer_t[DSHOT_DMA_SLOT_COUNT][DSHOT_TIMER_LANE_COUNT];

typedef enum {
    DSHOT_TIMING_OK = 0,
    DSHOT_TIMING_INVALID_ARGUMENT,
    DSHOT_TIMING_UNSUPPORTED_RATE,
    DSHOT_TIMING_UNSUPPORTED_TIMER_CLOCK,
    DSHOT_TIMING_INVALID_PROFILE,
} dshot_timing_result_t;

dshot_timing_result_t dshot_timing_profile_create(
    dshot_rate_t rate,
    uint32_t timer_clock_hz,
    dshot_timing_profile_t *profile);

/*
 * Frame index zero becomes the first interleaved DMA lane. For the selected
 * V1 TIM8 burst, lanes zero through three are CCR1 through CCR4 respectively.
 */
dshot_timing_result_t dshot_timing_build_dma_buffer(
    const dshot_timing_profile_t *profile,
    const uint16_t frames_by_timer_lane[DSHOT_TIMER_LANE_COUNT],
    dshot_dma_buffer_t buffer);

#endif
