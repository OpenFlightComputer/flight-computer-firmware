#include "timebase_snapshot.h"

bool timebase_snapshot_resolve(const timebase_snapshot_t *snapshot,
                               uint64_t *time_value_us)
{
    uint32_t effective_overflow;
    uint32_t effective_counter;

    if (snapshot->overflow_before != snapshot->overflow_after) {
        return false;
    }

    effective_overflow = snapshot->overflow_before;
    effective_counter = snapshot->counter_before;

    if (snapshot->update_pending) {
        effective_overflow++;
        effective_counter = snapshot->counter_after;
    }

    *time_value_us = ((uint64_t)effective_overflow << 32U) |
                     (uint64_t)effective_counter;
    return true;
}
