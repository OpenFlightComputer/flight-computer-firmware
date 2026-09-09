#include "board.h"

#include "board_definition.h"

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define STORAGE_MAGIC UINT32_C(0x4F464343)
#define STORAGE_FORMAT_VERSION UINT32_C(1)
#define STORAGE_COMMIT UINT32_C(0x434F4D54)
#define STORAGE_PAYLOAD_CAPACITY 16U

typedef struct {
    uint32_t magic;
    uint32_t format_version;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload[STORAGE_PAYLOAD_CAPACITY / sizeof(uint32_t)];
    uint32_t crc;
    uint32_t commit;
} storage_record_t;

_Static_assert((sizeof(storage_record_t) % sizeof(uint32_t)) == 0U,
               "Persistent records must use whole flash words");
_Static_assert((STORAGE_PAYLOAD_CAPACITY % sizeof(uint32_t)) == 0U,
               "Persistent payload capacity must use whole flash words");

static uint32_t crc32_update(uint32_t crc, uint8_t value)
{
    uint32_t bit;

    crc ^= value;
    for (bit = 0U; bit < 8U; bit++) {
        const uint32_t mask = UINT32_C(0) - (crc & UINT32_C(1));

        crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
    }
    return crc;
}

static uint32_t record_crc(const storage_record_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    const size_t protected_length = offsetof(storage_record_t, crc);
    uint32_t crc = UINT32_MAX;
    size_t index;

    for (index = 0U; index < protected_length; index++) {
        crc = crc32_update(crc, bytes[index]);
    }
    return ~crc;
}

static const storage_record_t *record_at(size_t index)
{
    const uintptr_t address =
        (uintptr_t)FLIGHTCOMPUTER_V1_CONFIGURATION_FLASH_ADDRESS +
        (index * sizeof(storage_record_t));

    return (const storage_record_t *)address;
}

static size_t record_capacity(void)
{
    return FLIGHTCOMPUTER_V1_CONFIGURATION_FLASH_LENGTH /
           sizeof(storage_record_t);
}

static bool record_is_erased(const storage_record_t *record)
{
    const volatile uint32_t *words =
        (const volatile uint32_t *)(const void *)record;
    size_t index;

    for (index = 0U; index < (sizeof(*record) / sizeof(uint32_t)); index++) {
        if (words[index] != UINT32_MAX) {
            return false;
        }
    }
    return true;
}

static bool record_is_valid(const storage_record_t *record, size_t length)
{
    storage_record_t snapshot;

    memcpy(&snapshot, record, sizeof(snapshot));
    return (snapshot.magic == STORAGE_MAGIC) &&
           (snapshot.format_version == STORAGE_FORMAT_VERSION) &&
           (snapshot.payload_length == length) &&
           (snapshot.payload_length <= STORAGE_PAYLOAD_CAPACITY) &&
           (snapshot.commit == STORAGE_COMMIT) &&
           (snapshot.crc == record_crc(&snapshot));
}

board_persistent_storage_read_result_t board_persistent_storage_read(
    void *destination,
    size_t length)
{
    const storage_record_t *latest = NULL;
    uint32_t latest_sequence = 0U;
    bool nonempty_seen = false;
    size_t index;

    if ((destination == NULL) || (length == 0U) ||
        (length > STORAGE_PAYLOAD_CAPACITY)) {
        return BOARD_PERSISTENT_STORAGE_READ_ERROR;
    }

    for (index = 0U; index < record_capacity(); index++) {
        const storage_record_t *record = record_at(index);

        if (record_is_erased(record)) {
            continue;
        }
        nonempty_seen = true;
        if (record_is_valid(record, length) &&
            ((latest == NULL) || (record->sequence > latest_sequence))) {
            latest = record;
            latest_sequence = record->sequence;
        }
    }

    if (latest == NULL) {
        return nonempty_seen ? BOARD_PERSISTENT_STORAGE_READ_ERROR
                             : BOARD_PERSISTENT_STORAGE_READ_EMPTY;
    }

    memcpy(destination, latest->payload, length);
    return BOARD_PERSISTENT_STORAGE_READ_OK;
}

board_persistent_storage_write_result_t board_persistent_storage_write(
    const void *source,
    size_t length)
{
    storage_record_t candidate;
    const storage_record_t *destination = NULL;
    uint32_t latest_sequence = 0U;
    size_t index;
    size_t word;

    if ((source == NULL) || (length == 0U) ||
        (length > STORAGE_PAYLOAD_CAPACITY)) {
        return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    }

    for (index = 0U; index < record_capacity(); index++) {
        const storage_record_t *record = record_at(index);

        if (record_is_erased(record)) {
            if (destination == NULL) {
                destination = record;
            }
        } else if (record_is_valid(record, length) &&
                   (record->sequence > latest_sequence)) {
            latest_sequence = record->sequence;
        }
    }
    if ((destination == NULL) || (latest_sequence == UINT32_MAX)) {
        return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    }

    candidate = (storage_record_t){
        .magic = STORAGE_MAGIC,
        .format_version = STORAGE_FORMAT_VERSION,
        .sequence = latest_sequence + 1U,
        .payload_length = (uint32_t)length,
        .payload = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX},
        .commit = STORAGE_COMMIT,
    };
    memcpy(candidate.payload, source, length);
    candidate.crc = record_crc(&candidate);

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    }
    for (word = 0U;
         word < (offsetof(storage_record_t, commit) / sizeof(uint32_t));
         word++) {
        const uint32_t value = ((const uint32_t *)&candidate)[word];
        const uintptr_t address = (uintptr_t)destination +
                                  (word * sizeof(uint32_t));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              (uint32_t)address,
                              value) != HAL_OK) {
            (void)HAL_FLASH_Lock();
            return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
        }
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          (uint32_t)(uintptr_t)&destination->commit,
                          candidate.commit) != HAL_OK) {
        (void)HAL_FLASH_Lock();
        return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    }
    if (HAL_FLASH_Lock() != HAL_OK) {
        return BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
    }

    return record_is_valid(destination, length)
               ? BOARD_PERSISTENT_STORAGE_WRITE_OK
               : BOARD_PERSISTENT_STORAGE_WRITE_ERROR;
}

board_persistent_storage_clear_result_t board_persistent_storage_clear(void)
{
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = FLIGHTCOMPUTER_V1_CONFIGURATION_FLASH_SECTOR,
        .NbSectors = 1U,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = UINT32_MAX;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    }
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK) {
        (void)HAL_FLASH_Lock();
        return BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    }
    if (HAL_FLASH_Lock() != HAL_OK) {
        return BOARD_PERSISTENT_STORAGE_CLEAR_ERROR;
    }

    return BOARD_PERSISTENT_STORAGE_CLEAR_OK;
}
