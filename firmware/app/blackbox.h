#ifndef OPENFLIGHTCOMPUTER_BLACKBOX_H
#define OPENFLIGHTCOMPUTER_BLACKBOX_H

#include "control_trace.h"
#include "sd_card.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLACKBOX_FORMAT_VERSION 2U
#define BLACKBOX_STORAGE_FORMAT_VERSION 1U
#define BLACKBOX_QUEUE_CAPACITY 32U
#define BLACKBOX_LOG_CAPACITY 16U
#define BLACKBOX_SAMPLE_RATE_HZ 100U
#define BLACKBOX_SAMPLE_INTERVAL_US UINT32_C(10000)
#define BLACKBOX_CONFIGURATION_CAPACITY 512U

typedef struct {
    uint32_t id;
    uint32_t start_sector;
    uint32_t end_sector;
    uint32_t sample_count;
    uint32_t dropped_sample_count;
    bool complete;
} blackbox_log_information_t;

typedef enum {
    BLACKBOX_STATUS_NO_MEDIA = 0,
    BLACKBOX_STATUS_UNINITIALIZED,
    BLACKBOX_STATUS_READY,
    BLACKBOX_STATUS_RECORDING,
    BLACKBOX_STATUS_FINISHING,
    BLACKBOX_STATUS_ERROR,
} blackbox_status_t;

typedef struct {
    uint8_t data[SD_CARD_SECTOR_SIZE];
    uint32_t target_sector;
} blackbox_queued_sector_t;

typedef struct {
    sd_card_t *card;
    uint8_t configuration[BLACKBOX_CONFIGURATION_CAPACITY];
    const char *firmware_version;
    const char *build_id;
    size_t configuration_length;
    blackbox_queued_sector_t queue[BLACKBOX_QUEUE_CAPACITY];
    blackbox_log_information_t logs[BLACKBOX_LOG_CAPACITY];
    uint8_t sample_block[SD_CARD_SECTOR_SIZE];
    uint64_t next_sample_at_us;
    uint64_t captured_sample_count;
    uint64_t dropped_sample_count;
    uint64_t completed_sector_write_count;
    uint64_t total_sector_write_time_us;
    uint64_t maximum_sector_write_time_us;
    uint64_t sector_write_started_at_us;
    uint64_t next_checkpoint_at_sample;
    uint64_t finish_after_us;
    uint32_t next_sector;
    uint32_t next_log_id;
    uint32_t generation;
    uint32_t active_log_index;
    uint32_t active_block_sequence;
    uint32_t queue_head;
    uint32_t queue_count;
    uint32_t maximum_queue_depth;
    uint8_t sample_count_in_block;
    blackbox_status_t status;
    system_state_t previous_system_state;
    bool sector_write_active;
    bool armed_seen;
    bool initialized;
} blackbox_t;

void blackbox_initialize(blackbox_t *blackbox,
                         sd_card_t *card,
                         const uint8_t *configuration,
                         size_t configuration_length,
                         const char *firmware_version,
                         const char *build_id);
bool blackbox_set_configuration(blackbox_t *blackbox,
                                const uint8_t *configuration,
                                size_t configuration_length);
bool blackbox_storage_initialize(blackbox_t *blackbox);
bool blackbox_capture_due(const blackbox_t *blackbox,
                          uint64_t timestamp_us,
                          system_state_t state);
void blackbox_capture(blackbox_t *blackbox,
                      const control_trace_sample_t *sample);
void blackbox_finish_recording(blackbox_t *blackbox,
                               const control_trace_sample_t *sample);
void blackbox_service(blackbox_t *blackbox);
size_t blackbox_log_count(const blackbox_t *blackbox);
bool blackbox_log_information(const blackbox_t *blackbox,
                              size_t index,
                              blackbox_log_information_t *information);
bool blackbox_read_log_sector(blackbox_t *blackbox,
                              uint32_t log_id,
                              uint32_t sector_offset,
                              uint8_t destination[SD_CARD_SECTOR_SIZE]);
const char *blackbox_status_name(blackbox_status_t status);

#endif
