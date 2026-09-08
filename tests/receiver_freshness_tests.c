#include "receiver_freshness.h"

#include <assert.h>
#include <string.h>

static const receiver_freshness_config_t config = {
    .fresh_through_us = 100U,
    .lost_after_us = 300U,
};

static void test_configuration_validation(void)
{
    receiver_freshness_config_t invalid = config;

    assert(receiver_freshness_config_is_valid(&config));
    assert(!receiver_freshness_config_is_valid(NULL));
    invalid.fresh_through_us = 0U;
    assert(!receiver_freshness_config_is_valid(&invalid));
    invalid = config;
    invalid.lost_after_us = invalid.fresh_through_us;
    assert(!receiver_freshness_config_is_valid(&invalid));
}

static void test_unavailable_and_invalid_configuration(void)
{
    receiver_freshness_config_t invalid = config;

    invalid.fresh_through_us = 0U;
    assert(receiver_freshness_evaluate(&config, false, 0U, 100U) ==
           RECEIVER_FRESHNESS_UNAVAILABLE);
    assert(receiver_freshness_evaluate(NULL, true, 0U, 100U) ==
           RECEIVER_FRESHNESS_UNAVAILABLE);
    assert(receiver_freshness_evaluate(&invalid, true, 0U, 100U) ==
           RECEIVER_FRESHNESS_UNAVAILABLE);
}

static void test_exact_fresh_stale_and_lost_boundaries(void)
{
    assert(receiver_freshness_evaluate(&config, true, 1000U, 1000U) ==
           RECEIVER_FRESHNESS_FRESH);
    assert(receiver_freshness_evaluate(&config, true, 1000U, 1100U) ==
           RECEIVER_FRESHNESS_FRESH);
    assert(receiver_freshness_evaluate(&config, true, 1000U, 1101U) ==
           RECEIVER_FRESHNESS_STALE);
    assert(receiver_freshness_evaluate(&config, true, 1000U, 1300U) ==
           RECEIVER_FRESHNESS_STALE);
    assert(receiver_freshness_evaluate(&config, true, 1000U, 1301U) ==
           RECEIVER_FRESHNESS_LOST);
}

static void test_clock_rollback_fails_closed(void)
{
    assert(receiver_freshness_evaluate(&config, true, 1000U, 999U) ==
           RECEIVER_FRESHNESS_LOST);
}

static void test_state_names(void)
{
    assert(strcmp(receiver_freshness_state_name(RECEIVER_FRESHNESS_UNAVAILABLE),
                  "UNAVAILABLE") == 0);
    assert(strcmp(receiver_freshness_state_name(RECEIVER_FRESHNESS_FRESH),
                  "FRESH") == 0);
    assert(strcmp(receiver_freshness_state_name(RECEIVER_FRESHNESS_STALE),
                  "STALE") == 0);
    assert(strcmp(receiver_freshness_state_name(RECEIVER_FRESHNESS_LOST),
                  "LOST") == 0);
    assert(strcmp(receiver_freshness_state_name(RECEIVER_FRESHNESS_COUNT),
                  "UNKNOWN") == 0);
}

int main(void)
{
    test_configuration_validation();
    test_unavailable_and_invalid_configuration();
    test_exact_fresh_stale_and_lost_boundaries();
    test_clock_rollback_fails_closed();
    test_state_names();
    return 0;
}
