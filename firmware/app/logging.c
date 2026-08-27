#include "logging.h"

#include "logging_config.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

logging_system_t firmware_logging_system;

typedef struct {
    uint64_t timestamp_us;
    bool valid;
} logging_time_t;

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static bool level_is_valid(log_level_t level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG:
    case LOG_LEVEL_INFO:
    case LOG_LEVEL_WARN:
    case LOG_LEVEL_ERROR:
    case LOG_LEVEL_FATAL:
        return true;
    case LOG_LEVEL_COUNT:
        break;
    }

    return false;
}

static bool threshold_is_valid(log_threshold_t threshold)
{
    switch (threshold) {
    case LOG_THRESHOLD_DEBUG:
    case LOG_THRESHOLD_INFO:
    case LOG_THRESHOLD_WARN:
    case LOG_THRESHOLD_ERROR:
    case LOG_THRESHOLD_FATAL:
    case LOG_THRESHOLD_OFF:
        return true;
    case LOG_THRESHOLD_COUNT:
        break;
    }

    return false;
}

static bool module_is_valid(log_module_t module)
{
    switch (module) {
    case LOG_MODULE_SYSTEM:
    case LOG_MODULE_BOARD:
    case LOG_MODULE_TIMEBASE:
    case LOG_MODULE_TASK:
    case LOG_MODULE_SCHEDULER:
    case LOG_MODULE_STATE:
    case LOG_MODULE_FAULT:
    case LOG_MODULE_USB:
        return true;
    case LOG_MODULE_COUNT:
        break;
    }

    return false;
}

static logging_time_t current_logging_time(void)
{
    if (firmware_logging_system.clock == NULL) {
        return (logging_time_t){0};
    }

    return (logging_time_t){
        .timestamp_us = firmware_logging_system.clock(),
        .valid = true,
    };
}

static log_threshold_t effective_threshold(log_module_t module)
{
    if (firmware_logging_system.module_override_enabled[module]) {
        return firmware_logging_system.module_thresholds[module];
    }

    return firmware_logging_system.global_threshold;
}

static uint64_t take_next_sequence(void)
{
    const uint64_t sequence = firmware_logging_system.next_sequence;

    firmware_logging_system.next_sequence++;
    return sequence;
}

static void pop_oldest_record(void)
{
    firmware_logging_system.records[firmware_logging_system.head] =
        (log_record_t){0};
    firmware_logging_system.head =
        (firmware_logging_system.head + 1U) % LOGGING_QUEUE_CAPACITY;
    firmware_logging_system.count--;
}

void logging_initialize(void)
{
    firmware_logging_system = (logging_system_t){
        .next_sequence = 1U,
        .global_threshold = logging_default_global_threshold(),
        .initialized = true,
    };
}

logging_clock_attach_result_t logging_attach_clock(logging_clock_t clock)
{
    if (!firmware_logging_system.initialized || (clock == NULL)) {
        return LOGGING_CLOCK_ATTACH_INVALID_ARGUMENT;
    }

    firmware_logging_system.clock = clock;
    return LOGGING_CLOCK_ATTACH_OK;
}

logging_config_result_t logging_set_global_threshold(log_threshold_t threshold)
{
    if (!firmware_logging_system.initialized ||
        !threshold_is_valid(threshold)) {
        return LOGGING_CONFIG_INVALID_ARGUMENT;
    }

    firmware_logging_system.global_threshold = threshold;
    return LOGGING_CONFIG_OK;
}

logging_config_result_t logging_set_module_threshold(log_module_t module,
                                                     log_threshold_t threshold)
{
    if (!firmware_logging_system.initialized || !module_is_valid(module) ||
        !threshold_is_valid(threshold)) {
        return LOGGING_CONFIG_INVALID_ARGUMENT;
    }

    firmware_logging_system.module_thresholds[module] = threshold;
    firmware_logging_system.module_override_enabled[module] = true;
    return LOGGING_CONFIG_OK;
}

logging_config_result_t logging_clear_module_threshold(log_module_t module)
{
    if (!firmware_logging_system.initialized || !module_is_valid(module)) {
        return LOGGING_CONFIG_INVALID_ARGUMENT;
    }

    firmware_logging_system.module_thresholds[module] = LOG_THRESHOLD_DEBUG;
    firmware_logging_system.module_override_enabled[module] = false;
    return LOGGING_CONFIG_OK;
}

bool logging_should_emit(log_level_t level, log_module_t module)
{
    log_threshold_t threshold;

    if (!firmware_logging_system.initialized || !level_is_valid(level) ||
        !module_is_valid(module)) {
        return false;
    }

    threshold = effective_threshold(module);
    if (threshold == LOG_THRESHOLD_OFF) {
        return false;
    }

    return (unsigned int)level >= (unsigned int)threshold;
}

void logging_note_filtered(log_level_t level, log_module_t module)
{
    if (firmware_logging_system.initialized && level_is_valid(level) &&
        module_is_valid(module) && !logging_should_emit(level, module)) {
        saturating_increment(&firmware_logging_system.statistics.filtered_count);
    }
}

logging_write_result_t logging_write(log_level_t level,
                                     log_module_t module,
                                     const char *format,
                                     ...)
{
    logging_time_t time;
    log_record_t *record;
    uint64_t sequence;
    size_t record_index;
    va_list arguments;
    int written;

    if (!firmware_logging_system.initialized || !level_is_valid(level) ||
        !module_is_valid(module) || (format == NULL)) {
        return LOGGING_WRITE_INVALID_ARGUMENT;
    }
    if (!logging_should_emit(level, module)) {
        saturating_increment(&firmware_logging_system.statistics.filtered_count);
        return LOGGING_WRITE_FILTERED;
    }

    sequence = take_next_sequence();
    if (firmware_logging_system.count >= LOGGING_QUEUE_CAPACITY) {
        saturating_increment(&firmware_logging_system.statistics.dropped_count);
        saturating_increment(
            &firmware_logging_system.statistics.dropped_by_level[level]);
        return LOGGING_WRITE_DROPPED_FULL;
    }

    time = current_logging_time();
    record_index = (firmware_logging_system.head +
                    firmware_logging_system.count) %
                   LOGGING_QUEUE_CAPACITY;
    record = &firmware_logging_system.records[record_index];
    *record = (log_record_t){0};

    va_start(arguments, format);
    written = vsnprintf(record->message,
                        sizeof(record->message),
                        format,
                        arguments);
    va_end(arguments);

    if (written < 0) {
        *record = (log_record_t){0};
        saturating_increment(
            &firmware_logging_system.statistics.format_error_count);
        return LOGGING_WRITE_FORMAT_ERROR;
    }

    record->timestamp_us = time.timestamp_us;
    record->sequence = sequence;
    record->level = level;
    record->module = module;
    record->timestamp_valid = time.valid;
    if ((size_t)written >= sizeof(record->message)) {
        record->message_length = (uint16_t)(sizeof(record->message) - 1U);
        record->truncated = true;
        saturating_increment(
            &firmware_logging_system.statistics.truncated_count);
    } else {
        record->message_length = (uint16_t)written;
    }

    firmware_logging_system.count++;
    saturating_increment(&firmware_logging_system.statistics.enqueued_count);
    return LOGGING_WRITE_ENQUEUED;
}

logging_backend_attach_result_t logging_attach_backend(
    const logging_backend_t *backend)
{
    if (!firmware_logging_system.initialized || (backend == NULL) ||
        (backend->try_write == NULL)) {
        return LOGGING_BACKEND_ATTACH_INVALID_ARGUMENT;
    }

    firmware_logging_system.backend = *backend;
    firmware_logging_system.backend_attached = true;
    return LOGGING_BACKEND_ATTACH_OK;
}

logging_drain_result_t logging_drain_once(void)
{
    const log_record_t *record;
    logging_backend_result_t backend_result;

    if (!firmware_logging_system.initialized) {
        return LOGGING_DRAIN_INVALID_STATE;
    }
    if (firmware_logging_system.count == 0U) {
        return LOGGING_DRAIN_IDLE;
    }
    if (!firmware_logging_system.backend_attached) {
        return LOGGING_DRAIN_NO_BACKEND;
    }

    record = &firmware_logging_system.records[firmware_logging_system.head];
    backend_result = firmware_logging_system.backend.try_write(
        record,
        firmware_logging_system.backend.context);

    switch (backend_result) {
    case LOG_BACKEND_ACCEPTED:
        pop_oldest_record();
        saturating_increment(
            &firmware_logging_system.statistics.drained_count);
        return LOGGING_DRAIN_ACCEPTED;
    case LOG_BACKEND_BUSY:
        saturating_increment(
            &firmware_logging_system.statistics.backend_busy_count);
        return LOGGING_DRAIN_BACKEND_BUSY;
    case LOG_BACKEND_ERROR:
    case LOG_BACKEND_RESULT_COUNT:
        pop_oldest_record();
        saturating_increment(
            &firmware_logging_system.statistics.backend_error_count);
        return LOGGING_DRAIN_BACKEND_ERROR;
    }

    pop_oldest_record();
    saturating_increment(
        &firmware_logging_system.statistics.backend_error_count);
    return LOGGING_DRAIN_BACKEND_ERROR;
}

const log_record_t *logging_peek(void)
{
    if (!firmware_logging_system.initialized ||
        (firmware_logging_system.count == 0U)) {
        return NULL;
    }

    return &firmware_logging_system.records[firmware_logging_system.head];
}

size_t logging_queue_count(void)
{
    if (!firmware_logging_system.initialized) {
        return 0U;
    }

    return firmware_logging_system.count;
}

logging_format_result_t logging_format_record(const log_record_t *record,
                                              char *output,
                                              size_t output_capacity,
                                              size_t *output_length)
{
    int written;

    if ((record == NULL) || (output == NULL) || (output_capacity == 0U) ||
        (output_length == NULL) || !level_is_valid(record->level) ||
        !module_is_valid(record->module) ||
        (record->message_length >= LOGGING_MESSAGE_CAPACITY) ||
        (record->message[record->message_length] != '\0')) {
        return LOGGING_FORMAT_INVALID_ARGUMENT;
    }

    if (record->timestamp_valid) {
        written = snprintf(output,
                           output_capacity,
                           "[%010llu] #%010llu %-5s %-9s %s\n",
                           (unsigned long long)record->timestamp_us,
                           (unsigned long long)record->sequence,
                           logging_level_name(record->level),
                           logging_module_name(record->module),
                           record->message);
    } else {
        written = snprintf(output,
                           output_capacity,
                           "[----------] #%010llu %-5s %-9s %s\n",
                           (unsigned long long)record->sequence,
                           logging_level_name(record->level),
                           logging_module_name(record->module),
                           record->message);
    }

    if (written < 0) {
        *output_length = 0U;
        return LOGGING_FORMAT_ERROR;
    }
    if ((size_t)written >= output_capacity) {
        *output_length = 0U;
        return LOGGING_FORMAT_BUFFER_TOO_SMALL;
    }

    *output_length = (size_t)written;
    return LOGGING_FORMAT_OK;
}
