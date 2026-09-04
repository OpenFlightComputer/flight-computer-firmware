#include "dshot_timing.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define DSHOT_DUTY_DENOMINATOR 8U
#define DSHOT_ZERO_HIGH_NUMERATOR 3U
#define DSHOT_ONE_HIGH_NUMERATOR 6U

static bool rate_is_supported(dshot_rate_t rate)
{
    return rate == DSHOT_RATE_300;
}

static dshot_timing_result_t calculate_profile(
    dshot_rate_t rate,
    uint32_t timer_clock_hz,
    dshot_timing_profile_t *profile)
{
    uint32_t bit_period_ticks;

    if (!rate_is_supported(rate)) {
        return DSHOT_TIMING_UNSUPPORTED_RATE;
    }
    if ((timer_clock_hz == 0U) ||
        ((timer_clock_hz % (uint32_t)rate) != 0U)) {
        return DSHOT_TIMING_UNSUPPORTED_TIMER_CLOCK;
    }

    bit_period_ticks = timer_clock_hz / (uint32_t)rate;
    if ((bit_period_ticks == 0U) ||
        (bit_period_ticks > UINT16_MAX) ||
        ((bit_period_ticks % DSHOT_DUTY_DENOMINATOR) != 0U)) {
        return DSHOT_TIMING_UNSUPPORTED_TIMER_CLOCK;
    }

    *profile = (dshot_timing_profile_t){
        .rate = rate,
        .timer_clock_hz = timer_clock_hz,
        .bit_period_ticks = (uint16_t)bit_period_ticks,
        .zero_high_ticks =
            (uint16_t)((bit_period_ticks / DSHOT_DUTY_DENOMINATOR) *
                       DSHOT_ZERO_HIGH_NUMERATOR),
        .one_high_ticks =
            (uint16_t)((bit_period_ticks / DSHOT_DUTY_DENOMINATOR) *
                       DSHOT_ONE_HIGH_NUMERATOR),
    };
    return DSHOT_TIMING_OK;
}

static bool profile_is_valid(const dshot_timing_profile_t *profile)
{
    dshot_timing_profile_t expected;

    if (calculate_profile(profile->rate,
                          profile->timer_clock_hz,
                          &expected) != DSHOT_TIMING_OK) {
        return false;
    }

    return (profile->bit_period_ticks == expected.bit_period_ticks) &&
           (profile->zero_high_ticks == expected.zero_high_ticks) &&
           (profile->one_high_ticks == expected.one_high_ticks);
}

dshot_timing_result_t dshot_timing_profile_create(
    dshot_rate_t rate,
    uint32_t timer_clock_hz,
    dshot_timing_profile_t *profile)
{
    dshot_timing_profile_t candidate;
    dshot_timing_result_t result;

    if (profile == NULL) {
        return DSHOT_TIMING_INVALID_ARGUMENT;
    }

    result = calculate_profile(rate, timer_clock_hz, &candidate);
    if (result != DSHOT_TIMING_OK) {
        return result;
    }

    *profile = candidate;
    return DSHOT_TIMING_OK;
}

dshot_timing_result_t dshot_timing_build_dma_buffer(
    const dshot_timing_profile_t *profile,
    const uint16_t frames_by_timer_lane[DSHOT_TIMER_LANE_COUNT],
    dshot_dma_buffer_t buffer)
{
    uint16_t owned_frames[DSHOT_TIMER_LANE_COUNT];
    size_t bit_index;
    size_t lane;

    if ((profile == NULL) || (frames_by_timer_lane == NULL) ||
        (buffer == NULL)) {
        return DSHOT_TIMING_INVALID_ARGUMENT;
    }
    if (!profile_is_valid(profile)) {
        return DSHOT_TIMING_INVALID_PROFILE;
    }

    for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
        owned_frames[lane] = frames_by_timer_lane[lane];
    }

    for (bit_index = 0U; bit_index < DSHOT_FRAME_BIT_COUNT; bit_index++) {
        const uint16_t bit_mask =
            (uint16_t)(UINT16_C(1) <<
                       (DSHOT_FRAME_BIT_COUNT - 1U - bit_index));

        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            buffer[bit_index][lane] =
                ((owned_frames[lane] & bit_mask) != 0U)
                    ? profile->one_high_ticks
                    : profile->zero_high_ticks;
        }
    }

    for (; bit_index < DSHOT_DMA_SLOT_COUNT; bit_index++) {
        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            buffer[bit_index][lane] = 0U;
        }
    }

    return DSHOT_TIMING_OK;
}
