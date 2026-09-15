#include "blackbox.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define BLACKBOX_MAGIC UINT32_C(0x4243464F) /* OFCB, little endian */
#define BLACKBOX_SUPERBLOCK_MAGIC UINT32_C(0x5343464F) /* OFCS */
#define BLACKBOX_BLOCK_HEADER_SIZE 20U
#define BLACKBOX_BLOCK_CRC_OFFSET 508U
#define BLACKBOX_BLOCK_PAYLOAD_CAPACITY \
    (BLACKBOX_BLOCK_CRC_OFFSET - BLACKBOX_BLOCK_HEADER_SIZE)
#define BLACKBOX_SAMPLE_SIZE 232U
#define BLACKBOX_SAMPLES_PER_BLOCK 2U
#define BLACKBOX_CHECKPOINT_SAMPLE_INTERVAL UINT64_C(500)
#define BLACKBOX_CONFIGURATION_CHUNK_SIZE BLACKBOX_BLOCK_PAYLOAD_CAPACITY
#define BLACKBOX_FIRST_DATA_SECTOR UINT32_C(2)
#define BLACKBOX_SUPERBLOCK_LOG_OFFSET 32U
#define BLACKBOX_SUPERBLOCK_LOG_SIZE 24U

typedef enum {
    BLACKBOX_BLOCK_FLIGHT_HEADER = 1,
    BLACKBOX_BLOCK_CONFIGURATION = 2,
    BLACKBOX_BLOCK_SAMPLES = 3,
    BLACKBOX_BLOCK_FLIGHT_FOOTER = 4,
} blackbox_block_type_t;

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t checksum = UINT32_MAX;
    size_t index;
    for (index = 0U; index < length; index++) {
        uint32_t bit;
        checksum ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            checksum = (checksum >> 1U) ^
                       ((checksum & 1U) != 0U ? UINT32_C(0xEDB88320) : 0U);
        }
    }
    return ~checksum;
}

static void put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
    destination[2] = (uint8_t)(value >> 16U);
    destination[3] = (uint8_t)(value >> 24U);
}

static void put_u64(uint8_t *destination, uint64_t value)
{
    put_u32(destination, (uint32_t)value);
    put_u32(destination + 4U, (uint32_t)(value >> 32U));
}

static uint32_t get_u32(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) |
           ((uint32_t)source[2] << 16U) | ((uint32_t)source[3] << 24U);
}

static void put_float(uint8_t *destination, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_u32(destination, bits);
}

static void put_string(uint8_t *destination, size_t capacity, const char *value)
{
    size_t length;
    if ((destination == NULL) || (capacity == 0U) || (value == NULL)) {
        return;
    }
    length = strlen(value);
    if (length >= capacity) {
        length = capacity - 1U;
    }
    memcpy(destination, value, length);
}

static bool superblock_valid(const uint8_t sector[SD_CARD_SECTOR_SIZE])
{
    return (get_u32(sector) == BLACKBOX_SUPERBLOCK_MAGIC) &&
           (get_u32(sector + 4U) == BLACKBOX_FORMAT_VERSION) &&
           (get_u32(sector + BLACKBOX_BLOCK_CRC_OFFSET) ==
            crc32(sector, BLACKBOX_BLOCK_CRC_OFFSET));
}

static void build_superblock(const blackbox_t *blackbox,
                             uint8_t sector[SD_CARD_SECTOR_SIZE])
{
    size_t index;
    memset(sector, 0, SD_CARD_SECTOR_SIZE);
    put_u32(sector, BLACKBOX_SUPERBLOCK_MAGIC);
    put_u32(sector + 4U, BLACKBOX_FORMAT_VERSION);
    put_u32(sector + 8U, blackbox->generation);
    put_u32(sector + 12U, blackbox->next_sector);
    put_u32(sector + 16U, blackbox->next_log_id);
    put_u32(sector + 20U, (uint32_t)blackbox_log_count(blackbox));
    for (index = 0U; index < BLACKBOX_LOG_CAPACITY; index++) {
        const size_t offset = BLACKBOX_SUPERBLOCK_LOG_OFFSET +
                              index * BLACKBOX_SUPERBLOCK_LOG_SIZE;
        const blackbox_log_information_t *log = &blackbox->logs[index];
        put_u32(sector + offset, log->id);
        put_u32(sector + offset + 4U, log->start_sector);
        put_u32(sector + offset + 8U, log->end_sector);
        put_u32(sector + offset + 12U, log->sample_count);
        put_u32(sector + offset + 16U, log->dropped_sample_count);
        put_u32(sector + offset + 20U, log->complete ? 1U : 0U);
    }
    put_u32(sector + BLACKBOX_BLOCK_CRC_OFFSET,
            crc32(sector, BLACKBOX_BLOCK_CRC_OFFSET));
}

static void load_superblock(blackbox_t *blackbox,
                            const uint8_t sector[SD_CARD_SECTOR_SIZE])
{
    size_t index;
    blackbox->generation = get_u32(sector + 8U);
    blackbox->next_sector = get_u32(sector + 12U);
    blackbox->next_log_id = get_u32(sector + 16U);
    for (index = 0U; index < BLACKBOX_LOG_CAPACITY; index++) {
        const size_t offset = BLACKBOX_SUPERBLOCK_LOG_OFFSET +
                              index * BLACKBOX_SUPERBLOCK_LOG_SIZE;
        blackbox->logs[index] = (blackbox_log_information_t){
            .id = get_u32(sector + offset),
            .start_sector = get_u32(sector + offset + 4U),
            .end_sector = get_u32(sector + offset + 8U),
            .sample_count = get_u32(sector + offset + 12U),
            .dropped_sample_count = get_u32(sector + offset + 16U),
            .complete = get_u32(sector + offset + 20U) != 0U,
        };
    }
}

static void finalize_block(uint8_t sector[SD_CARD_SECTOR_SIZE],
                           blackbox_block_type_t type,
                           uint32_t log_id,
                           uint32_t sequence,
                           uint16_t item_count,
                           uint16_t payload_length)
{
    put_u32(sector, BLACKBOX_MAGIC);
    put_u16(sector + 4U, BLACKBOX_FORMAT_VERSION);
    put_u16(sector + 6U, (uint16_t)type);
    put_u32(sector + 8U, log_id);
    put_u32(sector + 12U, sequence);
    put_u16(sector + 16U, item_count);
    put_u16(sector + 18U, payload_length);
    put_u32(sector + BLACKBOX_BLOCK_CRC_OFFSET,
            crc32(sector, BLACKBOX_BLOCK_CRC_OFFSET));
}

static bool queue_sector(blackbox_t *blackbox,
                         uint32_t target_sector,
                         const uint8_t sector[SD_CARD_SECTOR_SIZE])
{
    uint32_t index;
    if (blackbox->queue_count >= BLACKBOX_QUEUE_CAPACITY) {
        return false;
    }
    index = (blackbox->queue_head + blackbox->queue_count) %
            BLACKBOX_QUEUE_CAPACITY;
    blackbox->queue[index].target_sector = target_sector;
    memcpy(blackbox->queue[index].data, sector, SD_CARD_SECTOR_SIZE);
    blackbox->queue_count++;
    if (blackbox->queue_count > blackbox->maximum_queue_depth) {
        blackbox->maximum_queue_depth = blackbox->queue_count;
    }
    return true;
}

static bool allocate_and_queue(blackbox_t *blackbox,
                               uint8_t sector[SD_CARD_SECTOR_SIZE])
{
    if ((blackbox->next_sector >= blackbox->card->sector_count) ||
        !queue_sector(blackbox, blackbox->next_sector, sector)) {
        return false;
    }
    blackbox->logs[blackbox->active_log_index].end_sector =
        blackbox->next_sector;
    blackbox->next_sector++;
    return true;
}

static uint32_t active_log_id(const blackbox_t *blackbox)
{
    return blackbox->logs[blackbox->active_log_index].id;
}

static bool enqueue_superblocks(blackbox_t *blackbox)
{
    uint8_t sector[SD_CARD_SECTOR_SIZE];
    build_superblock(blackbox, sector);
    return queue_sector(blackbox, 0U, sector) &&
           queue_sector(blackbox, 1U, sector);
}

static bool start_flight(blackbox_t *blackbox, uint64_t timestamp_us)
{
    uint8_t sector[SD_CARD_SECTOR_SIZE];
    size_t log_index = blackbox_log_count(blackbox);
    size_t offset = 0U;
    uint32_t chunk_index = 0U;

    if ((log_index >= BLACKBOX_LOG_CAPACITY) ||
        (blackbox->next_sector >= blackbox->card->sector_count) ||
        (blackbox->queue_count != 0U)) {
        return false;
    }
    blackbox->captured_sample_count = 0U;
    blackbox->dropped_sample_count = 0U;
    blackbox->completed_sector_write_count = 0U;
    blackbox->total_sector_write_time_us = 0U;
    blackbox->maximum_sector_write_time_us = 0U;
    blackbox->sector_write_started_at_us = 0U;
    blackbox->maximum_queue_depth = 0U;
    blackbox->active_log_index = (uint32_t)log_index;
    blackbox->logs[log_index] = (blackbox_log_information_t){
        .id = blackbox->next_log_id,
        .start_sector = blackbox->next_sector,
    };
    if (blackbox->next_log_id < UINT32_MAX) {
        blackbox->next_log_id++;
    }
    memset(sector, 0, sizeof(sector));
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE, BLACKBOX_SAMPLE_INTERVAL_US);
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE + 4U,
            (uint32_t)blackbox->configuration_length);
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE + 8U,
            crc32(blackbox->configuration, blackbox->configuration_length));
    put_u64(sector + BLACKBOX_BLOCK_HEADER_SIZE + 12U, timestamp_us);
    put_string(sector + BLACKBOX_BLOCK_HEADER_SIZE + 20U,
               16U, blackbox->firmware_version);
    put_string(sector + BLACKBOX_BLOCK_HEADER_SIZE + 36U,
               32U, blackbox->build_id);
    finalize_block(sector, BLACKBOX_BLOCK_FLIGHT_HEADER,
                   active_log_id(blackbox), blackbox->active_block_sequence++,
                   1U, 68U);
    if (!allocate_and_queue(blackbox, sector)) {
        return false;
    }
    while (offset < blackbox->configuration_length) {
        const size_t remaining = blackbox->configuration_length - offset;
        const size_t chunk = remaining < BLACKBOX_CONFIGURATION_CHUNK_SIZE
                                 ? remaining
                                 : BLACKBOX_CONFIGURATION_CHUNK_SIZE;
        memset(sector, 0, sizeof(sector));
        memcpy(sector + BLACKBOX_BLOCK_HEADER_SIZE,
               blackbox->configuration + offset, chunk);
        finalize_block(sector, BLACKBOX_BLOCK_CONFIGURATION,
                       active_log_id(blackbox),
                       blackbox->active_block_sequence++,
                       (uint16_t)chunk_index++, (uint16_t)chunk);
        if (!allocate_and_queue(blackbox, sector)) {
            return false;
        }
        offset += chunk;
    }
    blackbox->generation++;
    if (!enqueue_superblocks(blackbox)) {
        return false;
    }
    blackbox->next_sample_at_us = timestamp_us;
    blackbox->next_checkpoint_at_sample =
        BLACKBOX_CHECKPOINT_SAMPLE_INTERVAL;
    blackbox->sample_count_in_block = 0U;
    blackbox->armed_seen = true;
    blackbox->status = BLACKBOX_STATUS_RECORDING;
    return true;
}

static void checkpoint_flight(blackbox_t *blackbox)
{
    blackbox_log_information_t *log;
    if ((blackbox->captured_sample_count <
         blackbox->next_checkpoint_at_sample) ||
        (blackbox->queue_count > BLACKBOX_QUEUE_CAPACITY - 2U)) {
        return;
    }
    log = &blackbox->logs[blackbox->active_log_index];
    log->sample_count = (uint32_t)blackbox->captured_sample_count;
    log->dropped_sample_count = (uint32_t)blackbox->dropped_sample_count;
    blackbox->generation++;
    if (enqueue_superblocks(blackbox)) {
        blackbox->next_checkpoint_at_sample +=
            BLACKBOX_CHECKPOINT_SAMPLE_INTERVAL;
    }
}

static uint32_t event_flags_for_sample(const control_trace_sample_t *sample)
{
    uint32_t flags = 0U;
    if (sample->system_state == SYSTEM_STATE_FAULT) {
        flags |= CONTROL_TRACE_EVENT_STATE_CHANGED;
    }
    if (sample->failsafe_action != RECEIVER_FAILSAFE_ACTION_LIVE) {
        flags |= CONTROL_TRACE_EVENT_FAILSAFE_CHANGED;
    }
    if (sample->mixer_output_valid && sample->mixer_output.saturated) {
        flags |= CONTROL_TRACE_EVENT_SATURATION_STARTED;
    }
    return flags;
}

static void encode_sample(const control_trace_sample_t *sample,
                          uint32_t sequence,
                          uint8_t destination[BLACKBOX_SAMPLE_SIZE])
{
    size_t axis;
    size_t motor;
    size_t offset = 0U;
    uint32_t statuses;
    uint32_t validity = 0U;

    memset(destination, 0, BLACKBOX_SAMPLE_SIZE);
    put_u64(destination + offset, sample->timestamp_us); offset += 8U;
    put_u64(destination + offset, sample->attitude.source_sequence); offset += 8U;
    put_u32(destination + offset, sample->rate_output.dt_us); offset += 4U;
    put_u32(destination + offset, event_flags_for_sample(sample)); offset += 4U;
    statuses = ((uint32_t)sample->system_state) |
               ((uint32_t)sample->control_source << 8U) |
               ((uint32_t)sample->failsafe_state << 16U) |
               ((uint32_t)sample->failsafe_action << 24U);
    put_u32(destination + offset, statuses); offset += 4U;
    statuses = ((uint32_t)sample->imu_freshness) |
               ((uint32_t)sample->control_result << 8U) |
               ((uint32_t)sample->rate_result << 16U);
    if (sample->receiver.valid) { validity |= UINT32_C(1) << 0U; }
    if (sample->setpoint.valid) { validity |= UINT32_C(1) << 1U; }
    if (sample->attitude.valid) { validity |= UINT32_C(1) << 2U; }
    if (sample->rate_output.valid) { validity |= UINT32_C(1) << 3U; }
    if (sample->mixer_output_valid) { validity |= UINT32_C(1) << 4U; }
    if (sample->mixer_output_valid && sample->mixer_output.saturated) {
        validity |= UINT32_C(1) << 5U;
    }
    put_u32(destination + offset, statuses); offset += 4U;
    put_u32(destination + offset, validity); offset += 4U;
    for (axis = 0U; axis < 3U; axis++) {
        put_u32(destination + offset,
                (uint32_t)sample->imu_observation.raw_acceleration[axis]);
        offset += 4U;
    }
    for (axis = 0U; axis < 3U; axis++) {
        put_u32(destination + offset,
                (uint32_t)sample->imu_observation.raw_gyroscope[axis]);
        offset += 4U;
    }
#define PUT_FLOAT_ARRAY(values, count)                                    \
    do {                                                                  \
        size_t array_index;                                               \
        for (array_index = 0U; array_index < (count); array_index++) {    \
            put_float(destination + offset, (values)[array_index]);       \
            offset += 4U;                                                 \
        }                                                                 \
    } while (0)
    PUT_FLOAT_ARRAY(sample->imu_observation.filtered_acceleration_g, 3U);
    PUT_FLOAT_ARRAY(sample->attitude.filtered_gyroscope_dps, 3U);
    PUT_FLOAT_ARRAY(
        sample->imu_observation.filtered_accelerometer_attitude_degrees, 2U);
    PUT_FLOAT_ARRAY(sample->imu_observation.gyro_predicted_attitude_degrees, 2U);
    put_float(destination + offset, sample->attitude.roll_degrees); offset += 4U;
    put_float(destination + offset, sample->attitude.pitch_degrees); offset += 4U;
    put_float(destination + offset,
              sample->imu_observation.accelerometer_weight); offset += 4U;
    put_float(destination + offset, sample->receiver.throttle); offset += 4U;
    put_float(destination + offset, sample->receiver.roll); offset += 4U;
    put_float(destination + offset, sample->receiver.pitch); offset += 4U;
    put_float(destination + offset, sample->receiver.yaw); offset += 4U;
    put_float(destination + offset, sample->setpoint.throttle); offset += 4U;
    put_float(destination + offset,
              sample->setpoint.desired_roll_degrees); offset += 4U;
    put_float(destination + offset,
              sample->setpoint.desired_pitch_degrees); offset += 4U;
    put_float(destination + offset,
              sample->setpoint.desired_yaw_rate_dps); offset += 4U;
    PUT_FLOAT_ARRAY(sample->desired_rates.desired_rate_dps, 3U);
    for (axis = 0U; axis < 3U; axis++) {
        put_float(destination + offset,
                  sample->rate_output.axis[axis].proportional); offset += 4U;
        put_float(destination + offset,
                  sample->rate_output.axis[axis].integral); offset += 4U;
        put_float(destination + offset,
                  sample->rate_output.axis[axis].derivative); offset += 4U;
        put_float(destination + offset,
                  sample->rate_output.axis[axis].total); offset += 4U;
    }
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        const float value = sample->mixer_output_valid
                                ? sample->mixer_output.command.throttle[motor]
                                : 0.0F;
        put_float(destination + offset, value); offset += 4U;
    }
    put_float(destination + offset,
              sample->mixer_output.correction_scale); offset += 4U;
    put_float(destination + offset,
              sample->mixer_output.collective_shift); offset += 4U;
    put_u32(destination + offset, sequence);
#undef PUT_FLOAT_ARRAY
}

static uint64_t sample_periods_due(blackbox_t *blackbox,
                                   uint64_t timestamp_us)
{
    uint64_t periods;
    uint64_t maximum_advance;

    if ((blackbox->next_sample_at_us == UINT64_MAX) ||
        (timestamp_us < blackbox->next_sample_at_us)) {
        return 0U;
    }
    periods = ((timestamp_us - blackbox->next_sample_at_us) /
               BLACKBOX_SAMPLE_INTERVAL_US) + 1U;
    maximum_advance = (UINT64_MAX - blackbox->next_sample_at_us) /
                      BLACKBOX_SAMPLE_INTERVAL_US;
    if (periods > maximum_advance) {
        blackbox->next_sample_at_us = UINT64_MAX;
    } else {
        blackbox->next_sample_at_us +=
            periods * BLACKBOX_SAMPLE_INTERVAL_US;
    }
    return periods;
}

static void add_dropped_samples(blackbox_t *blackbox, uint64_t count)
{
    const uint64_t remaining = UINT64_MAX - blackbox->dropped_sample_count;

    blackbox->dropped_sample_count += count > remaining ? remaining : count;
}

static bool flush_sample_block(blackbox_t *blackbox)
{
    const uint16_t payload_length =
        (uint16_t)(blackbox->sample_count_in_block * BLACKBOX_SAMPLE_SIZE);
    if (blackbox->sample_count_in_block == 0U) {
        return true;
    }
    finalize_block(blackbox->sample_block, BLACKBOX_BLOCK_SAMPLES,
                   active_log_id(blackbox), blackbox->active_block_sequence++,
                   blackbox->sample_count_in_block, payload_length);
    if (!allocate_and_queue(blackbox, blackbox->sample_block)) {
        return false;
    }
    blackbox->sample_count_in_block = 0U;
    memset(blackbox->sample_block, 0, sizeof(blackbox->sample_block));
    return true;
}

static void finish_flight(blackbox_t *blackbox, const control_trace_sample_t *sample)
{
    uint8_t sector[SD_CARD_SECTOR_SIZE];
    blackbox_log_information_t *log = &blackbox->logs[blackbox->active_log_index];

    if (!flush_sample_block(blackbox)) {
        blackbox->status = BLACKBOX_STATUS_ERROR;
        return;
    }
    memset(sector, 0, sizeof(sector));
    put_u64(sector + BLACKBOX_BLOCK_HEADER_SIZE, sample->timestamp_us);
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE + 8U,
            (uint32_t)blackbox->captured_sample_count);
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE + 12U,
            (uint32_t)blackbox->dropped_sample_count);
    put_u32(sector + BLACKBOX_BLOCK_HEADER_SIZE + 16U,
            (uint32_t)sample->system_state);
    finalize_block(sector, BLACKBOX_BLOCK_FLIGHT_FOOTER,
                   active_log_id(blackbox), blackbox->active_block_sequence++,
                   1U, 20U);
    if (allocate_and_queue(blackbox, sector)) {
        log->sample_count = (uint32_t)blackbox->captured_sample_count;
        log->dropped_sample_count = (uint32_t)blackbox->dropped_sample_count;
        log->complete = true;
        blackbox->generation++;
        if (!enqueue_superblocks(blackbox)) {
            blackbox->status = BLACKBOX_STATUS_ERROR;
            return;
        }
        blackbox->status = BLACKBOX_STATUS_FINISHING;
    } else {
        blackbox->status = BLACKBOX_STATUS_ERROR;
    }
}

void blackbox_initialize(blackbox_t *blackbox,
                         sd_card_t *card,
                         const uint8_t *configuration,
                         size_t configuration_length,
                         const char *firmware_version,
                         const char *build_id)
{
    uint8_t primary[SD_CARD_SECTOR_SIZE];
    uint8_t backup[SD_CARD_SECTOR_SIZE];
    bool primary_valid;
    bool backup_valid;

    if (blackbox == NULL) {
        return;
    }
    *blackbox = (blackbox_t){
        .card = card,
        .firmware_version = firmware_version,
        .build_id = build_id,
        .next_sector = BLACKBOX_FIRST_DATA_SECTOR,
        .next_log_id = 1U,
        .status = BLACKBOX_STATUS_NO_MEDIA,
        .initialized = true,
    };
    (void)blackbox_set_configuration(blackbox, configuration,
                                     configuration_length);
    if ((card == NULL) || !card->initialized) {
        return;
    }
    blackbox->status = BLACKBOX_STATUS_UNINITIALIZED;
    primary_valid = (sd_card_read_sector(card, 0U, primary) ==
                     SD_CARD_RESULT_OK) && superblock_valid(primary);
    backup_valid = (sd_card_read_sector(card, 1U, backup) ==
                    SD_CARD_RESULT_OK) && superblock_valid(backup);
    if (primary_valid || backup_valid) {
        const uint8_t *newest = primary_valid ? primary : backup;
        if (primary_valid && backup_valid &&
            (get_u32(backup + 8U) > get_u32(primary + 8U))) {
            newest = backup;
        }
        load_superblock(blackbox, newest);
        if ((blackbox->next_sector >= BLACKBOX_FIRST_DATA_SECTOR) &&
            ((uint64_t)blackbox->next_sector < card->sector_count)) {
            blackbox->status = BLACKBOX_STATUS_READY;
        } else {
            blackbox->status = BLACKBOX_STATUS_ERROR;
        }
    }
}

bool blackbox_set_configuration(blackbox_t *blackbox,
                                const uint8_t *configuration,
                                size_t configuration_length)
{
    if ((blackbox == NULL) ||
        ((configuration == NULL) && (configuration_length != 0U)) ||
        (configuration_length > sizeof(blackbox->configuration)) ||
        ((blackbox->status != BLACKBOX_STATUS_NO_MEDIA) &&
         (blackbox->status != BLACKBOX_STATUS_UNINITIALIZED) &&
         (blackbox->status != BLACKBOX_STATUS_READY))) {
        return false;
    }
    memset(blackbox->configuration, 0, sizeof(blackbox->configuration));
    if (configuration_length > 0U) {
        memcpy(blackbox->configuration, configuration, configuration_length);
    }
    blackbox->configuration_length = configuration_length;
    return true;
}

bool blackbox_storage_initialize(blackbox_t *blackbox)
{
    uint8_t sector[SD_CARD_SECTOR_SIZE];
    if ((blackbox == NULL) || !blackbox->initialized ||
        (blackbox->card == NULL) || !blackbox->card->initialized ||
        (blackbox->status == BLACKBOX_STATUS_RECORDING) ||
        (blackbox->status == BLACKBOX_STATUS_FINISHING)) {
        return false;
    }
    memset(blackbox->logs, 0, sizeof(blackbox->logs));
    blackbox->next_sector = BLACKBOX_FIRST_DATA_SECTOR;
    blackbox->next_log_id = 1U;
    blackbox->generation++;
    build_superblock(blackbox, sector);
    if ((sd_card_write_sector_blocking(blackbox->card, 0U, sector) !=
         SD_CARD_RESULT_OK) ||
        (sd_card_write_sector_blocking(blackbox->card, 1U, sector) !=
         SD_CARD_RESULT_OK)) {
        blackbox->status = BLACKBOX_STATUS_ERROR;
        return false;
    }
    blackbox->status = BLACKBOX_STATUS_READY;
    return true;
}

void blackbox_capture(blackbox_t *blackbox,
                      const control_trace_sample_t *sample)
{
    uint8_t *destination;
    uint64_t periods_due;
    bool stop_after;

    if ((blackbox == NULL) || !blackbox->initialized || (sample == NULL) ||
        (blackbox->status == BLACKBOX_STATUS_NO_MEDIA) ||
        (blackbox->status == BLACKBOX_STATUS_UNINITIALIZED) ||
        (blackbox->status == BLACKBOX_STATUS_ERROR)) {
        return;
    }
    if ((blackbox->status == BLACKBOX_STATUS_READY) &&
        (sample->system_state == SYSTEM_STATE_ARMED)) {
        blackbox->active_block_sequence = 0U;
        if (!start_flight(blackbox, sample->timestamp_us)) {
            blackbox->status = BLACKBOX_STATUS_ERROR;
            return;
        }
    }
    if (blackbox->status != BLACKBOX_STATUS_RECORDING) {
        return;
    }
    stop_after = blackbox->armed_seen &&
                 ((sample->system_state == SYSTEM_STATE_DISARMED) ||
                  (sample->system_state == SYSTEM_STATE_FAILSAFE) ||
                  (sample->system_state == SYSTEM_STATE_FAULT));
    periods_due = sample_periods_due(blackbox, sample->timestamp_us);
    if ((periods_due > 0U) || stop_after) {
        if (periods_due > 1U) {
            add_dropped_samples(blackbox, periods_due - 1U);
        }
        if (blackbox->queue_count >= BLACKBOX_QUEUE_CAPACITY - 4U) {
            add_dropped_samples(blackbox, 1U);
        } else {
            destination = blackbox->sample_block + BLACKBOX_BLOCK_HEADER_SIZE +
                          blackbox->sample_count_in_block * BLACKBOX_SAMPLE_SIZE;
            encode_sample(sample,
                          (uint32_t)(blackbox->captured_sample_count + 1U),
                          destination);
            blackbox->sample_count_in_block++;
            blackbox->captured_sample_count++;
            if (blackbox->sample_count_in_block >=
                    BLACKBOX_SAMPLES_PER_BLOCK &&
                !flush_sample_block(blackbox)) {
                blackbox->status = BLACKBOX_STATUS_ERROR;
                return;
            }
            checkpoint_flight(blackbox);
        }
    }
    if (stop_after) {
        finish_flight(blackbox, sample);
    }
}

void blackbox_service(blackbox_t *blackbox)
{
    blackbox_queued_sector_t *queued;
    sd_card_result_t result;

    if ((blackbox == NULL) || !blackbox->initialized ||
        (blackbox->queue_count == 0U) ||
        (blackbox->status == BLACKBOX_STATUS_ERROR)) {
        if ((blackbox != NULL) &&
            (blackbox->status == BLACKBOX_STATUS_FINISHING) &&
            (blackbox->queue_count == 0U)) {
            blackbox->status = BLACKBOX_STATUS_READY;
            blackbox->armed_seen = false;
        }
        return;
    }
    queued = &blackbox->queue[blackbox->queue_head];
    if (!blackbox->sector_write_active) {
        result = sd_card_write_sector_start(
            blackbox->card, queued->target_sector, queued->data);
        if (result == SD_CARD_RESULT_BUSY) {
            return;
        }
        if (result != SD_CARD_RESULT_OK) {
            blackbox->status = BLACKBOX_STATUS_ERROR;
            return;
        }
        blackbox->sector_write_active = true;
        blackbox->sector_write_started_at_us =
            blackbox->card->clock != NULL ? blackbox->card->clock() : 0U;
        return;
    }
    result = sd_card_write_sector_poll(blackbox->card);
    if (result == SD_CARD_RESULT_BUSY) {
        return;
    }
    blackbox->sector_write_active = false;
    if (result != SD_CARD_RESULT_OK) {
        blackbox->status = BLACKBOX_STATUS_ERROR;
        return;
    }
    if (blackbox->card->clock != NULL) {
        const uint64_t finished_at_us = blackbox->card->clock();
        const uint64_t elapsed =
            finished_at_us - blackbox->sector_write_started_at_us;
        const uint64_t remaining =
            UINT64_MAX - blackbox->total_sector_write_time_us;

        blackbox->total_sector_write_time_us +=
            elapsed > remaining ? remaining : elapsed;
        if (elapsed > blackbox->maximum_sector_write_time_us) {
            blackbox->maximum_sector_write_time_us = elapsed;
        }
    }
    if (blackbox->completed_sector_write_count < UINT64_MAX) {
        blackbox->completed_sector_write_count++;
    }
    blackbox->sector_write_started_at_us = 0U;
    memset(queued, 0, sizeof(*queued));
    blackbox->queue_head =
        (blackbox->queue_head + 1U) % BLACKBOX_QUEUE_CAPACITY;
    blackbox->queue_count--;
    if ((blackbox->status == BLACKBOX_STATUS_FINISHING) &&
        (blackbox->queue_count == 0U)) {
        blackbox->status = BLACKBOX_STATUS_READY;
        blackbox->armed_seen = false;
    }
}

size_t blackbox_log_count(const blackbox_t *blackbox)
{
    size_t count = 0U;
    if (blackbox == NULL) {
        return 0U;
    }
    while ((count < BLACKBOX_LOG_CAPACITY) &&
           (blackbox->logs[count].id != 0U)) {
        count++;
    }
    return count;
}

bool blackbox_log_information(const blackbox_t *blackbox,
                              size_t index,
                              blackbox_log_information_t *information)
{
    if ((blackbox == NULL) || (information == NULL) ||
        (index >= blackbox_log_count(blackbox))) {
        return false;
    }
    *information = blackbox->logs[index];
    return true;
}

bool blackbox_read_log_sector(blackbox_t *blackbox,
                              uint32_t log_id,
                              uint32_t sector_offset,
                              uint8_t destination[SD_CARD_SECTOR_SIZE])
{
    size_t index;
    if ((blackbox == NULL) || (destination == NULL) ||
        (blackbox->status != BLACKBOX_STATUS_READY)) {
        return false;
    }
    for (index = 0U; index < blackbox_log_count(blackbox); index++) {
        const blackbox_log_information_t *log = &blackbox->logs[index];
        const uint32_t sector_count = log->end_sector - log->start_sector + 1U;
        if ((log->id == log_id) && (sector_offset < sector_count)) {
            return sd_card_read_sector(blackbox->card,
                                       log->start_sector + sector_offset,
                                       destination) == SD_CARD_RESULT_OK;
        }
    }
    return false;
}

const char *blackbox_status_name(blackbox_status_t status)
{
    switch (status) {
    case BLACKBOX_STATUS_NO_MEDIA: return "NO_MEDIA";
    case BLACKBOX_STATUS_UNINITIALIZED: return "UNINITIALIZED";
    case BLACKBOX_STATUS_READY: return "READY";
    case BLACKBOX_STATUS_RECORDING: return "RECORDING";
    case BLACKBOX_STATUS_FINISHING: return "FINISHING";
    case BLACKBOX_STATUS_ERROR: return "ERROR";
    }
    return "INVALID";
}
