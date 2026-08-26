#include "timebase_snapshot.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void resolves_normal_counter_read(void)
{
    const timebase_snapshot_t snapshot = {
        .overflow_before = 7U,
        .counter_before = 1234U,
        .update_pending = false,
        .counter_after = 1235U,
        .overflow_after = 7U,
    };
    uint64_t result = 0U;

    assert(timebase_snapshot_resolve(&snapshot, &result));
    assert(result == (((uint64_t)7U << 32U) | 1234U));
}

static void accounts_for_pending_overflow(void)
{
    const timebase_snapshot_t snapshot = {
        .overflow_before = 7U,
        .counter_before = UINT32_MAX,
        .update_pending = true,
        .counter_after = 12U,
        .overflow_after = 7U,
    };
    uint64_t result = 0U;

    assert(timebase_snapshot_resolve(&snapshot, &result));
    assert(result == (((uint64_t)8U << 32U) | 12U));
}

static void requests_retry_when_interrupt_updates_overflow(void)
{
    const timebase_snapshot_t snapshot = {
        .overflow_before = 7U,
        .counter_before = UINT32_MAX,
        .update_pending = true,
        .counter_after = 12U,
        .overflow_after = 8U,
    };
    uint64_t result = UINT64_MAX;

    assert(!timebase_snapshot_resolve(&snapshot, &result));
    assert(result == UINT64_MAX);
}

static void returns_pre_wrap_sample_when_wrap_follows_flag_read(void)
{
    const timebase_snapshot_t snapshot = {
        .overflow_before = 7U,
        .counter_before = UINT32_MAX,
        .update_pending = false,
        .counter_after = 0U,
        .overflow_after = 7U,
    };
    uint64_t result = 0U;

    assert(timebase_snapshot_resolve(&snapshot, &result));
    assert(result == (((uint64_t)7U << 32U) | UINT32_MAX));
}

static void remains_monotonic_across_wrap(void)
{
    const timebase_snapshot_t before_wrap = {
        .overflow_before = 21U,
        .counter_before = UINT32_MAX,
        .update_pending = false,
        .counter_after = UINT32_MAX,
        .overflow_after = 21U,
    };
    const timebase_snapshot_t after_wrap = {
        .overflow_before = 21U,
        .counter_before = 0U,
        .update_pending = true,
        .counter_after = 1U,
        .overflow_after = 21U,
    };
    uint64_t before = 0U;
    uint64_t after = 0U;

    assert(timebase_snapshot_resolve(&before_wrap, &before));
    assert(timebase_snapshot_resolve(&after_wrap, &after));
    assert(after > before);
    assert((after - before) == 2U);
}

int main(void)
{
    resolves_normal_counter_read();
    accounts_for_pending_overflow();
    requests_retry_when_interrupt_updates_overflow();
    returns_pre_wrap_sample_when_wrap_follows_flag_read();
    remains_monotonic_across_wrap();
    return 0;
}
