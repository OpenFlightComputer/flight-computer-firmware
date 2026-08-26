#ifndef OPENFLIGHTCOMPUTER_TIMEBASE_SNAPSHOT_H
#define OPENFLIGHTCOMPUTER_TIMEBASE_SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t overflow_before;
    uint32_t counter_before;
    bool update_pending;
    uint32_t counter_after;
    uint32_t overflow_after;
} timebase_snapshot_t;

bool timebase_snapshot_resolve(const timebase_snapshot_t *snapshot,
                               uint64_t *time_value_us);

#endif
