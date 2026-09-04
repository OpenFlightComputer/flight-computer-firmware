#include "dshot_timing.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define V1_TIM8_CLOCK_HZ UINT32_C(168000000)
#define DSHOT300_PERIOD_TICKS UINT16_C(560)
#define DSHOT300_ZERO_HIGH_TICKS UINT16_C(210)
#define DSHOT300_ONE_HIGH_TICKS UINT16_C(420)

static dshot_timing_profile_t make_v1_profile(void)
{
    dshot_timing_profile_t profile;

    assert(dshot_timing_profile_create(DSHOT_RATE_300,
                                       V1_TIM8_CLOCK_HZ,
                                       &profile) == DSHOT_TIMING_OK);
    return profile;
}

static void v1_dshot300_timing_is_exact(void)
{
    const dshot_timing_profile_t profile = make_v1_profile();

    assert(profile.rate == DSHOT_RATE_300);
    assert(profile.timer_clock_hz == V1_TIM8_CLOCK_HZ);
    assert(profile.bit_period_ticks == DSHOT300_PERIOD_TICKS);
    assert(profile.zero_high_ticks == DSHOT300_ZERO_HIGH_TICKS);
    assert(profile.one_high_ticks == DSHOT300_ONE_HIGH_TICKS);
}

static void unsupported_profiles_are_rejected_atomically(void)
{
    const dshot_timing_profile_t sentinel = {
        .rate = DSHOT_RATE_300,
        .timer_clock_hz = UINT32_C(1),
        .bit_period_ticks = UINT16_C(2),
        .zero_high_ticks = UINT16_C(3),
        .one_high_ticks = UINT16_C(4),
    };
    dshot_timing_profile_t profile = sentinel;

    assert(dshot_timing_profile_create((dshot_rate_t)600000,
                                       V1_TIM8_CLOCK_HZ,
                                       &profile) ==
           DSHOT_TIMING_UNSUPPORTED_RATE);
    assert(profile.rate == sentinel.rate);
    assert(profile.timer_clock_hz == sentinel.timer_clock_hz);
    assert(profile.bit_period_ticks == sentinel.bit_period_ticks);
    assert(profile.zero_high_ticks == sentinel.zero_high_ticks);
    assert(profile.one_high_ticks == sentinel.one_high_ticks);

    assert(dshot_timing_profile_create(DSHOT_RATE_300,
                                       UINT32_C(168000001),
                                       &profile) ==
           DSHOT_TIMING_UNSUPPORTED_TIMER_CLOCK);
    assert(profile.timer_clock_hz == sentinel.timer_clock_hz);
    assert(dshot_timing_profile_create(DSHOT_RATE_300, 0U, &profile) ==
           DSHOT_TIMING_UNSUPPORTED_TIMER_CLOCK);
    assert(profile.timer_clock_hz == sentinel.timer_clock_hz);
    assert(dshot_timing_profile_create(DSHOT_RATE_300,
                                       V1_TIM8_CLOCK_HZ,
                                       NULL) == DSHOT_TIMING_INVALID_ARGUMENT);
}

static void known_four_motor_table_is_interleaved_msb_first(void)
{
    const dshot_timing_profile_t profile = make_v1_profile();
    /* V1 lanes are CCR1/M4, CCR2/M3, CCR3/M2, CCR4/M1. */
    const uint16_t frames_by_ccr[DSHOT_TIMER_LANE_COUNT] = {
        UINT16_C(0x4488),
        UINT16_C(0x4488),
        UINT16_C(0x830B),
        UINT16_C(0x830B),
    };
    const uint16_t expected[DSHOT_FRAME_BIT_COUNT]
                           [DSHOT_TIMER_LANE_COUNT] = {
        {210U, 210U, 420U, 420U},
        {420U, 420U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {420U, 420U, 210U, 210U},
        {210U, 210U, 420U, 420U},
        {210U, 210U, 420U, 420U},
        {420U, 420U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {210U, 210U, 210U, 210U},
        {420U, 420U, 420U, 420U},
        {210U, 210U, 210U, 210U},
        {210U, 210U, 420U, 420U},
        {210U, 210U, 420U, 420U},
    };
    dshot_dma_buffer_t buffer;
    size_t slot;
    size_t lane;

    assert(dshot_timing_build_dma_buffer(&profile,
                                         frames_by_ccr,
                                         buffer) == DSHOT_TIMING_OK);

    for (slot = 0U; slot < DSHOT_FRAME_BIT_COUNT; slot++) {
        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            assert(buffer[slot][lane] == expected[slot][lane]);
        }
    }
    for (; slot < DSHOT_DMA_SLOT_COUNT; slot++) {
        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            assert(buffer[slot][lane] == 0U);
        }
    }
}

static void every_bit_and_lane_selects_the_correct_duty(void)
{
    const dshot_timing_profile_t profile = make_v1_profile();
    size_t selected_lane;
    size_t selected_bit;

    for (selected_lane = 0U;
         selected_lane < DSHOT_TIMER_LANE_COUNT;
         selected_lane++) {
        for (selected_bit = 0U;
             selected_bit < DSHOT_FRAME_BIT_COUNT;
             selected_bit++) {
            uint16_t frames[DSHOT_TIMER_LANE_COUNT] = {0U};
            dshot_dma_buffer_t buffer;
            size_t slot;
            size_t lane;

            frames[selected_lane] =
                (uint16_t)(UINT16_C(1) <<
                           (DSHOT_FRAME_BIT_COUNT - 1U - selected_bit));
            assert(dshot_timing_build_dma_buffer(&profile,
                                                 frames,
                                                 buffer) == DSHOT_TIMING_OK);

            for (slot = 0U; slot < DSHOT_FRAME_BIT_COUNT; slot++) {
                for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
                    const uint16_t expected =
                        ((lane == selected_lane) &&
                         (slot == selected_bit))
                            ? DSHOT300_ONE_HIGH_TICKS
                            : DSHOT300_ZERO_HIGH_TICKS;
                    assert(buffer[slot][lane] == expected);
                }
            }
        }
    }
}

static void source_frames_may_overlap_the_destination(void)
{
    const dshot_timing_profile_t profile = make_v1_profile();
    const uint16_t expected_frames[DSHOT_TIMER_LANE_COUNT] = {
        UINT16_C(0x8000),
        UINT16_C(0x4000),
        UINT16_C(0x2000),
        UINT16_C(0x1000),
    };
    dshot_dma_buffer_t buffer = {{0U}};
    size_t lane;

    for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
        buffer[0][lane] = expected_frames[lane];
    }

    assert(dshot_timing_build_dma_buffer(&profile,
                                         buffer[0],
                                         buffer) == DSHOT_TIMING_OK);
    for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
        size_t slot;

        for (slot = 0U; slot < DSHOT_FRAME_BIT_COUNT; slot++) {
            assert(buffer[slot][lane] ==
                   ((slot == lane) ? DSHOT300_ONE_HIGH_TICKS
                                   : DSHOT300_ZERO_HIGH_TICKS));
        }
    }
}

static void invalid_inputs_do_not_modify_the_buffer(void)
{
    dshot_timing_profile_t profile = make_v1_profile();
    const uint16_t frames[DSHOT_TIMER_LANE_COUNT] = {0U};
    dshot_dma_buffer_t buffer;
    size_t slot;
    size_t lane;

    for (slot = 0U; slot < DSHOT_DMA_SLOT_COUNT; slot++) {
        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            buffer[slot][lane] = UINT16_C(0xA55A);
        }
    }

    profile.one_high_ticks++;
    assert(dshot_timing_build_dma_buffer(&profile, frames, buffer) ==
           DSHOT_TIMING_INVALID_PROFILE);
    for (slot = 0U; slot < DSHOT_DMA_SLOT_COUNT; slot++) {
        for (lane = 0U; lane < DSHOT_TIMER_LANE_COUNT; lane++) {
            assert(buffer[slot][lane] == UINT16_C(0xA55A));
        }
    }

    profile = make_v1_profile();
    assert(dshot_timing_build_dma_buffer(NULL, frames, buffer) ==
           DSHOT_TIMING_INVALID_ARGUMENT);
    assert(dshot_timing_build_dma_buffer(&profile, NULL, buffer) ==
           DSHOT_TIMING_INVALID_ARGUMENT);
    assert(dshot_timing_build_dma_buffer(&profile, frames, NULL) ==
           DSHOT_TIMING_INVALID_ARGUMENT);
}

int main(void)
{
    v1_dshot300_timing_is_exact();
    unsupported_profiles_are_rejected_atomically();
    known_four_motor_table_is_interleaved_msb_first();
    every_bit_and_lane_selects_the_correct_duty();
    source_frames_may_overlap_the_destination();
    invalid_inputs_do_not_modify_the_buffer();
    return 0;
}
