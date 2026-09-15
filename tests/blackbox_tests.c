#include "blackbox.h"

#include <assert.h>
#include <string.h>

#define TEST_SECTOR_COUNT 1024U

static uint8_t storage[TEST_SECTOR_COUNT][SD_CARD_SECTOR_SIZE];
static uint8_t pending[SD_CARD_SECTOR_SIZE];
static uint32_t pending_sector;
static bool write_active;
static uint64_t fake_time_us;

static uint64_t fake_clock(void)
{
    return fake_time_us;
}

sd_card_result_t sd_card_read_sector(
    sd_card_t *card,
    uint32_t sector,
    uint8_t destination[SD_CARD_SECTOR_SIZE])
{
    if (!card->initialized || (sector >= TEST_SECTOR_COUNT) || write_active) {
        return SD_CARD_RESULT_BUSY;
    }
    memcpy(destination, storage[sector], SD_CARD_SECTOR_SIZE);
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_start(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE])
{
    if (!card->initialized || write_active || (sector >= TEST_SECTOR_COUNT)) {
        return SD_CARD_RESULT_BUSY;
    }
    memcpy(pending, source, sizeof(pending));
    pending_sector = sector;
    write_active = true;
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_poll(sd_card_t *card)
{
    (void)card;
    if (!write_active) {
        return SD_CARD_RESULT_OK;
    }
    memcpy(storage[pending_sector], pending, sizeof(pending));
    write_active = false;
    fake_time_us += 2500U;
    return SD_CARD_RESULT_OK;
}

sd_card_result_t sd_card_write_sector_blocking(
    sd_card_t *card,
    uint32_t sector,
    const uint8_t source[SD_CARD_SECTOR_SIZE])
{
    const sd_card_result_t result =
        sd_card_write_sector_start(card, sector, source);
    return result == SD_CARD_RESULT_OK
               ? sd_card_write_sector_poll(card)
               : result;
}

static control_trace_sample_t sample(uint64_t timestamp_us,
                                     system_state_t state)
{
    control_trace_sample_t result = {
        .timestamp_us = timestamp_us,
        .system_state = state,
        .control_result = FLIGHT_CONTROL_SUBMITTED,
        .rate_result = RATE_CONTROLLER_RESULT_UPDATED,
        .receiver = {.valid = true, .throttle = 0.25F},
        .setpoint = {.valid = true, .throttle = 0.25F},
        .attitude = {.valid = true, .source_sequence = 1U},
        .rate_output = {.valid = true, .dt_us = 1000U},
        .mixer_output_valid = true,
    };
    result.mixer_output.command.throttle[0] = 0.25F;
    return result;
}

static void service_until_idle(blackbox_t *blackbox)
{
    size_t attempts = 0U;
    while ((blackbox->queue_count > 0U) && (attempts++ < 1000U)) {
        blackbox_service(blackbox);
    }
    assert(blackbox->queue_count == 0U);
}

int main(void)
{
    sd_card_t card = {
        .clock = fake_clock,
        .sector_count = TEST_SECTOR_COUNT,
        .initialized = true,
    };
    uint8_t configuration[BLACKBOX_CONFIGURATION_CAPACITY];
    blackbox_t blackbox;
    blackbox_t mounted;
    blackbox_log_information_t log;
    control_trace_sample_t current;

    memset(storage, 0, sizeof(storage));
    memset(configuration, 0xA5, sizeof(configuration));
    blackbox_initialize(&blackbox, &card, configuration,
                        sizeof(configuration), "0.1.0", "test-build");
    assert(blackbox.status == BLACKBOX_STATUS_UNINITIALIZED);
    assert(blackbox_storage_initialize(&blackbox));
    assert(blackbox.status == BLACKBOX_STATUS_READY);

    current = sample(100000U, SYSTEM_STATE_ARMED);
    blackbox_capture(&blackbox, &current);
    assert(blackbox.status == BLACKBOX_STATUS_RECORDING);
    current = sample(100000U + BLACKBOX_SAMPLE_INTERVAL_US,
                     SYSTEM_STATE_ARMED);
    blackbox_capture(&blackbox, &current);
    current = sample(100000U + BLACKBOX_SAMPLE_INTERVAL_US +
                         BLACKBOX_SAMPLE_INTERVAL_US / 2U,
                     SYSTEM_STATE_FAILSAFE);
    blackbox_capture(&blackbox, &current);
    assert(blackbox.status == BLACKBOX_STATUS_FINISHING);
    service_until_idle(&blackbox);
    assert(blackbox.status == BLACKBOX_STATUS_READY);
    assert(blackbox_log_count(&blackbox) == 1U);
    assert(blackbox_log_information(&blackbox, 0U, &log));
    assert(log.id == 1U);
    assert(log.complete);
    assert(log.sample_count == 3U);
    assert(log.dropped_sample_count == 0U);
    assert(blackbox.maximum_queue_depth > 0U);
    assert(blackbox.completed_sector_write_count > 0U);
    assert(blackbox.maximum_sector_write_time_us == 2500U);
    assert(blackbox.total_sector_write_time_us ==
           blackbox.completed_sector_write_count * 2500U);

    blackbox_initialize(&mounted, &card, configuration,
                        sizeof(configuration), "0.1.0", "test-build");
    assert(mounted.status == BLACKBOX_STATUS_READY);
    assert(blackbox_log_information(&mounted, 0U, &log));
    assert(log.complete && (log.sample_count == 3U));

    current = sample(200000U, SYSTEM_STATE_ARMED);
    blackbox_capture(&mounted, &current);
    service_until_idle(&mounted);
    current = sample(200000U + 3U * BLACKBOX_SAMPLE_INTERVAL_US,
                     SYSTEM_STATE_ARMED);
    blackbox_capture(&mounted, &current);
    blackbox_service(&mounted);
    blackbox_service(&mounted);
    for (uint32_t index = 4U; index < 502U; index++) {
        current = sample(200000U +
                             (uint64_t)index * BLACKBOX_SAMPLE_INTERVAL_US,
                         SYSTEM_STATE_ARMED);
        blackbox_capture(&mounted, &current);
        blackbox_service(&mounted);
        blackbox_service(&mounted);
    }
    service_until_idle(&mounted);
    blackbox_initialize(&blackbox, &card, configuration,
                        sizeof(configuration), "0.1.0", "test-build");
    assert(blackbox.status == BLACKBOX_STATUS_READY);
    assert(blackbox_log_information(&blackbox, 1U, &log));
    assert(!log.complete);
    assert(log.sample_count == 500U);
    assert(log.dropped_sample_count == 2U);
    return 0;
}
