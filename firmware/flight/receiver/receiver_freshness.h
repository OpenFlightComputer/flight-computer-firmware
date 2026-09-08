#ifndef OPENFLIGHTCOMPUTER_RECEIVER_FRESHNESS_H
#define OPENFLIGHTCOMPUTER_RECEIVER_FRESHNESS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t fresh_through_us;
    uint64_t lost_after_us;
} receiver_freshness_config_t;

typedef enum {
    RECEIVER_FRESHNESS_UNAVAILABLE = 0,
    RECEIVER_FRESHNESS_FRESH,
    RECEIVER_FRESHNESS_STALE,
    RECEIVER_FRESHNESS_LOST,
    RECEIVER_FRESHNESS_COUNT,
} receiver_freshness_state_t;

bool receiver_freshness_config_is_valid(
    const receiver_freshness_config_t *config);
receiver_freshness_state_t receiver_freshness_evaluate(
    const receiver_freshness_config_t *config,
    bool snapshot_valid,
    uint64_t received_at_us,
    uint64_t now_us);
const char *receiver_freshness_state_name(receiver_freshness_state_t state);

#endif
