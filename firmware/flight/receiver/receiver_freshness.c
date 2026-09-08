#include "receiver_freshness.h"

#include <stddef.h>

bool receiver_freshness_config_is_valid(
    const receiver_freshness_config_t *config)
{
    return (config != NULL) && (config->fresh_through_us > 0U) &&
           (config->fresh_through_us < config->lost_after_us);
}

receiver_freshness_state_t receiver_freshness_evaluate(
    const receiver_freshness_config_t *config,
    bool snapshot_valid,
    uint64_t received_at_us,
    uint64_t now_us)
{
    uint64_t age_us;

    if (!receiver_freshness_config_is_valid(config) || !snapshot_valid) {
        return RECEIVER_FRESHNESS_UNAVAILABLE;
    }
    if (now_us < received_at_us) {
        return RECEIVER_FRESHNESS_LOST;
    }

    age_us = now_us - received_at_us;
    if (age_us <= config->fresh_through_us) {
        return RECEIVER_FRESHNESS_FRESH;
    }
    if (age_us <= config->lost_after_us) {
        return RECEIVER_FRESHNESS_STALE;
    }
    return RECEIVER_FRESHNESS_LOST;
}

const char *receiver_freshness_state_name(receiver_freshness_state_t state)
{
    switch (state) {
    case RECEIVER_FRESHNESS_UNAVAILABLE:
        return "UNAVAILABLE";
    case RECEIVER_FRESHNESS_FRESH:
        return "FRESH";
    case RECEIVER_FRESHNESS_STALE:
        return "STALE";
    case RECEIVER_FRESHNESS_LOST:
        return "LOST";
    case RECEIVER_FRESHNESS_COUNT:
        break;
    }
    return "UNKNOWN";
}
