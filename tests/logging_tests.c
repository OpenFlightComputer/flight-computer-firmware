#include "logging.h"
#include "logging_config.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BACKEND_CAPTURE_CAPACITY 80U

typedef struct {
    logging_backend_result_t result;
    log_record_t captured[BACKEND_CAPTURE_CAPACITY];
    size_t captured_count;
    size_t call_count;
} fake_backend_context_t;

static uint64_t fake_time_us;

static uint64_t fake_clock(void)
{
    return fake_time_us;
}

static logging_backend_result_t fake_backend_try_write(
    const log_record_t *record,
    void *context)
{
    fake_backend_context_t *backend = context;

    backend->call_count++;
    if (backend->result == LOG_BACKEND_ACCEPTED) {
        assert(backend->captured_count < BACKEND_CAPTURE_CAPACITY);
        backend->captured[backend->captured_count] = *record;
        backend->captured_count++;
    }

    return backend->result;
}

static logging_backend_t backend_for(fake_backend_context_t *context)
{
    return (logging_backend_t){
        .try_write = fake_backend_try_write,
        .context = context,
    };
}

static void initializes_default_configuration(void)
{
    logging_initialize();

    assert(firmware_logging_system.initialized);
    assert(firmware_logging_system.global_threshold == LOG_THRESHOLD_INFO);
    assert(firmware_logging_system.next_sequence == 1U);
    assert(!firmware_logging_system.backend_attached);
    assert(logging_queue_count() == 0U);
    assert(logging_peek() == NULL);
    assert(strcmp(logging_level_name(LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(logging_level_name(LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(logging_level_name(LOG_LEVEL_WARN), "WARN") == 0);
    assert(strcmp(logging_level_name(LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(logging_level_name(LOG_LEVEL_FATAL), "FATAL") == 0);
    assert(strcmp(logging_level_name(LOG_LEVEL_COUNT), "UNKNOWN") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_SYSTEM), "SYSTEM") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_BOARD), "BOARD") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_TIMEBASE), "TIMEBASE") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_TASK), "TASK") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_SCHEDULER), "SCHEDULER") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_STATE), "STATE") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_FAULT), "FAULT") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_USB), "USB") == 0);
    assert(strcmp(logging_module_name(LOG_MODULE_COUNT), "UNKNOWN") == 0);
}

static void filters_globally_and_per_module(void)
{
    int evaluated = 0;

    logging_initialize();
    assert(!logging_should_emit(LOG_LEVEL_DEBUG, LOG_MODULE_SYSTEM));
    assert(logging_should_emit(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM));
    assert(logging_should_emit(LOG_LEVEL_FATAL, LOG_MODULE_SYSTEM));

    LOG_DEBUG(LOG_MODULE_SYSTEM, "value=%d", ++evaluated);
    assert(evaluated == 0);
    assert(firmware_logging_system.statistics.filtered_count == 1U);

    LOG_INFO(LOG_MODULE_SYSTEM, "value=%d", ++evaluated);
    assert(evaluated == 1);
    assert(logging_queue_count() == 1U);

    assert(logging_set_global_threshold(LOG_THRESHOLD_WARN) ==
           LOGGING_CONFIG_OK);
    assert(!logging_should_emit(LOG_LEVEL_INFO, LOG_MODULE_BOARD));
    assert(logging_should_emit(LOG_LEVEL_WARN, LOG_MODULE_BOARD));

    assert(logging_set_module_threshold(LOG_MODULE_BOARD,
                                        LOG_THRESHOLD_DEBUG) ==
           LOGGING_CONFIG_OK);
    assert(logging_should_emit(LOG_LEVEL_DEBUG, LOG_MODULE_BOARD));
    assert(!logging_should_emit(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM));

    assert(logging_set_module_threshold(LOG_MODULE_FAULT,
                                        LOG_THRESHOLD_OFF) ==
           LOGGING_CONFIG_OK);
    assert(!logging_should_emit(LOG_LEVEL_FATAL, LOG_MODULE_FAULT));
    assert(logging_clear_module_threshold(LOG_MODULE_BOARD) ==
           LOGGING_CONFIG_OK);
    assert(!logging_should_emit(LOG_LEVEL_INFO, LOG_MODULE_BOARD));

    assert(logging_set_global_threshold(LOG_THRESHOLD_OFF) ==
           LOGGING_CONFIG_OK);
    assert(!logging_should_emit(LOG_LEVEL_FATAL, LOG_MODULE_SYSTEM));

    assert(logging_set_global_threshold(LOG_THRESHOLD_COUNT) ==
           LOGGING_CONFIG_INVALID_ARGUMENT);
    assert(logging_set_module_threshold(LOG_MODULE_COUNT,
                                        LOG_THRESHOLD_DEBUG) ==
           LOGGING_CONFIG_INVALID_ARGUMENT);
    assert(logging_clear_module_threshold(LOG_MODULE_COUNT) ==
           LOGGING_CONFIG_INVALID_ARGUMENT);
    assert(!logging_should_emit((log_level_t)-1, LOG_MODULE_SYSTEM));
    assert(!logging_should_emit(LOG_LEVEL_INFO, (log_module_t)-1));
}

static void captures_time_sequence_and_bounded_messages(void)
{
    char long_message[LOGGING_MESSAGE_CAPACITY + 20U];
    const log_record_t *record;
    size_t index;

    logging_initialize();
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_SYSTEM,
                         "boot") == LOGGING_WRITE_ENQUEUED);
    record = logging_peek();
    assert(record != NULL);
    assert(record->sequence == 1U);
    assert(!record->timestamp_valid);
    assert(record->timestamp_us == 0U);
    assert(strcmp(record->message, "boot") == 0);

    assert(logging_attach_clock(fake_clock) == LOGGING_CLOCK_ATTACH_OK);
    fake_time_us = 25U;
    assert(logging_write(LOG_LEVEL_WARN,
                         LOG_MODULE_TIMEBASE,
                         "counter=%u",
                         7U) == LOGGING_WRITE_ENQUEUED);
    record = &firmware_logging_system.records[1];
    assert(record->sequence == 2U);
    assert(record->timestamp_valid);
    assert(record->timestamp_us == 25U);
    assert(strcmp(record->message, "counter=7") == 0);

    for (index = 0U; index < sizeof(long_message) - 1U; index++) {
        long_message[index] = 'x';
    }
    long_message[sizeof(long_message) - 1U] = '\0';
    assert(logging_write(LOG_LEVEL_ERROR,
                         LOG_MODULE_SYSTEM,
                         "%s",
                         long_message) == LOGGING_WRITE_ENQUEUED);
    record = &firmware_logging_system.records[2];
    assert(record->sequence == 3U);
    assert(record->truncated);
    assert(record->message_length == LOGGING_MESSAGE_CAPACITY - 1U);
    assert(record->message[LOGGING_MESSAGE_CAPACITY - 1U] == '\0');
    assert(firmware_logging_system.statistics.truncated_count == 1U);
}

static void formats_canonical_lines(void)
{
    log_record_t record = {
        .sequence = 1U,
        .level = LOG_LEVEL_INFO,
        .module = LOG_MODULE_SYSTEM,
        .message_length = 4U,
        .message = "boot",
    };
    char output[LOGGING_FORMATTED_LINE_CAPACITY];
    char small_output[8];
    size_t output_length = 99U;

    assert(logging_format_record(&record,
                                 output,
                                 sizeof(output),
                                 &output_length) == LOGGING_FORMAT_OK);
    assert(strcmp(output,
                  "[----------] #0000000001 INFO  SYSTEM    boot\n") == 0);
    assert(output_length == strlen(output));

    record.timestamp_valid = true;
    record.timestamp_us = 8121U;
    record.sequence = 42U;
    record.level = LOG_LEVEL_DEBUG;
    record.module = LOG_MODULE_SCHEDULER;
    record.message_length = 3U;
    memcpy(record.message, "run", 4U);
    assert(logging_format_record(&record,
                                 output,
                                 sizeof(output),
                                 &output_length) == LOGGING_FORMAT_OK);
    assert(strcmp(output,
                  "[0000008121] #0000000042 DEBUG SCHEDULER run\n") == 0);

    record.timestamp_us = UINT64_MAX;
    record.sequence = UINT64_MAX;
    assert(logging_format_record(&record,
                                 output,
                                 sizeof(output),
                                 &output_length) == LOGGING_FORMAT_OK);
    assert(strcmp(output,
                  "[18446744073709551615] #18446744073709551615 DEBUG "
                  "SCHEDULER run\n") == 0);

    assert(logging_format_record(&record,
                                 small_output,
                                 sizeof(small_output),
                                 &output_length) ==
           LOGGING_FORMAT_BUFFER_TOO_SMALL);
    assert(output_length == 0U);
    assert(logging_format_record(NULL,
                                 output,
                                 sizeof(output),
                                 &output_length) ==
           LOGGING_FORMAT_INVALID_ARGUMENT);
    record.message_length = LOGGING_MESSAGE_CAPACITY;
    assert(logging_format_record(&record,
                                 output,
                                 sizeof(output),
                                 &output_length) ==
           LOGGING_FORMAT_INVALID_ARGUMENT);
}

static void queue_is_fifo_across_wrap_and_counts_overflow(void)
{
    fake_backend_context_t backend_context = {
        .result = LOG_BACKEND_ACCEPTED,
    };
    logging_backend_t backend = backend_for(&backend_context);
    size_t index;

    logging_initialize();
    assert(logging_attach_backend(&backend) == LOGGING_BACKEND_ATTACH_OK);

    for (index = 0U; index < LOGGING_QUEUE_CAPACITY; index++) {
        assert(logging_write(LOG_LEVEL_INFO,
                             LOG_MODULE_TASK,
                             "record=%u",
                             (unsigned int)index) == LOGGING_WRITE_ENQUEUED);
    }
    assert(logging_write(LOG_LEVEL_WARN,
                         LOG_MODULE_TASK,
                         "dropped") == LOGGING_WRITE_DROPPED_FULL);
    assert(firmware_logging_system.statistics.dropped_count == 1U);
    assert(firmware_logging_system.statistics
               .dropped_by_level[LOG_LEVEL_WARN] == 1U);

    for (index = 0U; index < 5U; index++) {
        assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    }
    for (index = 0U; index < 5U; index++) {
        assert(logging_write(LOG_LEVEL_INFO,
                             LOG_MODULE_TASK,
                             "wrapped=%u",
                             (unsigned int)index) == LOGGING_WRITE_ENQUEUED);
    }
    while (logging_queue_count() > 0U) {
        assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    }

    assert(backend_context.captured_count == LOGGING_QUEUE_CAPACITY + 5U);
    for (index = 0U; index < LOGGING_QUEUE_CAPACITY; index++) {
        assert(backend_context.captured[index].sequence == index + 1U);
    }
    for (index = 0U; index < 5U; index++) {
        assert(backend_context.captured[LOGGING_QUEUE_CAPACITY + index]
                   .sequence == LOGGING_QUEUE_CAPACITY + 2U + index);
    }
    assert(firmware_logging_system.statistics.drained_count ==
           LOGGING_QUEUE_CAPACITY + 5U);
}

static void backend_busy_retries_and_errors_drop_one_record(void)
{
    fake_backend_context_t backend_context = {
        .result = LOG_BACKEND_BUSY,
    };
    logging_backend_t backend = backend_for(&backend_context);

    logging_initialize();
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_SYSTEM,
                         "one") == LOGGING_WRITE_ENQUEUED);
    assert(logging_drain_once() == LOGGING_DRAIN_NO_BACKEND);
    assert(logging_queue_count() == 1U);
    assert(logging_attach_backend(NULL) ==
           LOGGING_BACKEND_ATTACH_INVALID_ARGUMENT);
    backend.try_write = NULL;
    assert(logging_attach_backend(&backend) ==
           LOGGING_BACKEND_ATTACH_INVALID_ARGUMENT);

    backend = backend_for(&backend_context);
    assert(logging_attach_backend(&backend) == LOGGING_BACKEND_ATTACH_OK);
    assert(logging_drain_once() == LOGGING_DRAIN_BACKEND_BUSY);
    assert(logging_queue_count() == 1U);
    assert(firmware_logging_system.statistics.backend_busy_count == 1U);

    backend_context.result = LOG_BACKEND_ACCEPTED;
    assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    assert(logging_queue_count() == 0U);
    assert(backend_context.captured_count == 1U);
    assert(strcmp(backend_context.captured[0].message, "one") == 0);

    assert(logging_write(LOG_LEVEL_ERROR,
                         LOG_MODULE_SYSTEM,
                         "two") == LOGGING_WRITE_ENQUEUED);
    backend_context.result = LOG_BACKEND_ERROR;
    assert(logging_drain_once() == LOGGING_DRAIN_BACKEND_ERROR);
    assert(logging_queue_count() == 0U);
    assert(firmware_logging_system.statistics.backend_error_count == 1U);

    assert(logging_write(LOG_LEVEL_ERROR,
                         LOG_MODULE_SYSTEM,
                         "three") == LOGGING_WRITE_ENQUEUED);
    backend_context.result = (logging_backend_result_t)-1;
    assert(logging_drain_once() == LOGGING_DRAIN_BACKEND_ERROR);
    assert(logging_queue_count() == 0U);
    assert(firmware_logging_system.statistics.backend_error_count == 2U);
    assert(logging_drain_once() == LOGGING_DRAIN_IDLE);
}

static void rejects_invalid_writes_and_saturates_statistics(void)
{
    size_t index;

    firmware_logging_system = (logging_system_t){0};
    assert(logging_attach_clock(fake_clock) ==
           LOGGING_CLOCK_ATTACH_INVALID_ARGUMENT);
    assert(logging_set_global_threshold(LOG_THRESHOLD_INFO) ==
           LOGGING_CONFIG_INVALID_ARGUMENT);
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_SYSTEM,
                         "not initialized") ==
           LOGGING_WRITE_INVALID_ARGUMENT);
    assert(logging_drain_once() == LOGGING_DRAIN_INVALID_STATE);

    logging_initialize();
    assert(logging_write(LOG_LEVEL_COUNT,
                         LOG_MODULE_SYSTEM,
                         "invalid") == LOGGING_WRITE_INVALID_ARGUMENT);
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_COUNT,
                         "invalid") == LOGGING_WRITE_INVALID_ARGUMENT);
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_SYSTEM,
                         NULL) == LOGGING_WRITE_INVALID_ARGUMENT);

    firmware_logging_system.statistics.filtered_count = UINT32_MAX;
    LOG_DEBUG(LOG_MODULE_SYSTEM, "filtered");
    assert(firmware_logging_system.statistics.filtered_count == UINT32_MAX);

    for (index = 0U; index < LOGGING_QUEUE_CAPACITY; index++) {
        assert(logging_write(LOG_LEVEL_INFO,
                             LOG_MODULE_SYSTEM,
                             "fill") == LOGGING_WRITE_ENQUEUED);
    }
    firmware_logging_system.statistics.dropped_count = UINT32_MAX;
    firmware_logging_system.statistics.dropped_by_level[LOG_LEVEL_INFO] =
        UINT32_MAX;
    assert(logging_write(LOG_LEVEL_INFO,
                         LOG_MODULE_SYSTEM,
                         "drop") == LOGGING_WRITE_DROPPED_FULL);
    assert(firmware_logging_system.statistics.dropped_count == UINT32_MAX);
    assert(firmware_logging_system.statistics
               .dropped_by_level[LOG_LEVEL_INFO] == UINT32_MAX);
}

int main(void)
{
    initializes_default_configuration();
    filters_globally_and_per_module();
    captures_time_sequence_and_bounded_messages();
    formats_canonical_lines();
    queue_is_fifo_across_wrap_and_counts_overflow();
    backend_busy_retries_and_errors_drop_one_record();
    rejects_invalid_writes_and_saturates_statistics();
    return 0;
}
