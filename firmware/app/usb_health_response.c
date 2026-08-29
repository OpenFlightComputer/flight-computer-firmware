#include "usb_health_response.h"

#include "system_state.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define HEALTH_FAULT_JSON_CAPACITY 256U
#define HEALTH_SUFFIX_CAPACITY 96U

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool valid;
} json_buffer_t;

static void append_bytes(json_buffer_t *buffer,
                         const char *data,
                         size_t length)
{
    if (!buffer->valid || (length >= buffer->capacity - buffer->length)) {
        buffer->valid = false;
        return;
    }

    memcpy(&buffer->data[buffer->length], data, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void append_format(json_buffer_t *buffer, const char *format, ...)
{
    va_list arguments;
    int written;

    if (!buffer->valid || (buffer->length >= buffer->capacity)) {
        buffer->valid = false;
        return;
    }

    va_start(arguments, format);
    written = vsnprintf(&buffer->data[buffer->length],
                        buffer->capacity - buffer->length,
                        format,
                        arguments);
    va_end(arguments);

    if ((written < 0) ||
        ((size_t)written >= buffer->capacity - buffer->length)) {
        buffer->valid = false;
        return;
    }

    buffer->length += (size_t)written;
}

static bool format_fault(const fault_record_t *record,
                         char *destination,
                         size_t capacity,
                         size_t *length)
{
    json_buffer_t buffer = {
        .data = destination,
        .capacity = capacity,
        .valid = (destination != NULL) && (capacity > 0U),
    };

    if ((record == NULL) || !record->active || (destination == NULL) ||
        (capacity == 0U) || (length == NULL)) {
        return false;
    }

    destination[0] = '\0';
    append_format(&buffer,
                  "{\"id\":%u,\"severity\":\"%s\",\"source\":\"%s\","
                  "\"occurrence_count\":%lu,\"first_timestamp_us\":",
                  (unsigned int)record->id,
                  fault_severity_name(record->severity),
                  fault_source_name(record->source),
                  (unsigned long)record->occurrence_count);
    if (record->first_timestamp_valid) {
        append_format(&buffer,
                      "%llu",
                      (unsigned long long)record->first_timestamp_us);
    } else {
        append_bytes(&buffer, "null", 4U);
    }
    append_bytes(&buffer, ",\"last_timestamp_us\":", 21U);
    if (record->last_timestamp_valid) {
        append_format(&buffer,
                      "%llu",
                      (unsigned long long)record->last_timestamp_us);
    } else {
        append_bytes(&buffer, "null", 4U);
    }
    append_bytes(&buffer, ",\"context\":", 11U);
    if (record->context_valid) {
        append_format(&buffer, "%lu", (unsigned long)record->context);
    } else {
        append_bytes(&buffer, "null", 4U);
    }
    append_bytes(&buffer, "}", 1U);

    if (!buffer.valid) {
        *length = 0U;
        return false;
    }

    *length = buffer.length;
    return true;
}

static bool format_suffix(size_t reported_fault_count,
                          bool truncated,
                          char *destination,
                          size_t capacity,
                          size_t *length)
{
    const int written = snprintf(
        destination,
        capacity,
        "],\"reported_fault_count\":%lu,\"truncated\":%s}\n",
        (unsigned long)reported_fault_count,
        truncated ? "true" : "false");

    if ((written < 0) || ((size_t)written >= capacity)) {
        *length = 0U;
        return false;
    }

    *length = (size_t)written;
    return true;
}

bool usb_health_response_build(const health_summary_t *summary,
                               const fault_system_t *fault_system,
                               char *destination,
                               size_t capacity,
                               size_t *length)
{
    json_buffer_t buffer = {
        .data = destination,
        .capacity = capacity,
        .valid = (destination != NULL) && (capacity > 0U),
    };
    char fault_json[HEALTH_FAULT_JSON_CAPACITY];
    char suffix[HEALTH_SUFFIX_CAPACITY];
    size_t fault_json_length;
    size_t suffix_length;
    size_t reported_fault_count = 0U;
    size_t index;

    if ((summary == NULL) || (fault_system == NULL) ||
        !fault_system->initialized || (fault_system->state_machine == NULL) ||
        !fault_system->state_machine->initialized || (destination == NULL) ||
        (capacity == 0U) || (length == NULL)) {
        return false;
    }

    destination[0] = '\0';
    append_format(
        &buffer,
        "{\"type\":\"response\",\"command\":\"health\",\"ok\":true,"
        "\"health\":\"%s\",\"state\":\"%s\","
        "\"fault_data_complete\":%s,\"active_fault_count\":%lu,"
        "\"warning_count\":%lu,\"fault_count\":%lu,"
        "\"critical_count\":%lu,\"dropped_fault_count\":%lu,"
        "\"faults\":[",
        health_state_name(summary->state),
        system_state_name(fault_system->state_machine->current),
        summary->fault_data_complete ? "true" : "false",
        (unsigned long)summary->active_fault_count,
        (unsigned long)summary->warning_count,
        (unsigned long)summary->fault_count,
        (unsigned long)summary->critical_count,
        (unsigned long)summary->dropped_fault_count);
    if (!buffer.valid) {
        *length = 0U;
        return false;
    }

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        const fault_record_t *record = &fault_system->records[index];
        const size_t separator_length = reported_fault_count > 0U ? 1U : 0U;
        const size_t candidate_count = reported_fault_count + 1U;

        if (!record->active) {
            continue;
        }
        if (!format_fault(record,
                          fault_json,
                          sizeof(fault_json),
                          &fault_json_length) ||
            !format_suffix(candidate_count,
                           candidate_count < summary->active_fault_count,
                           suffix,
                           sizeof(suffix),
                           &suffix_length)) {
            *length = 0U;
            return false;
        }
        if ((separator_length + fault_json_length + suffix_length) >=
            buffer.capacity - buffer.length) {
            break;
        }

        if (separator_length > 0U) {
            append_bytes(&buffer, ",", 1U);
        }
        append_bytes(&buffer, fault_json, fault_json_length);
        reported_fault_count = candidate_count;
    }

    if (!format_suffix(reported_fault_count,
                       reported_fault_count < summary->active_fault_count,
                       suffix,
                       sizeof(suffix),
                       &suffix_length)) {
        *length = 0U;
        return false;
    }
    append_bytes(&buffer, suffix, suffix_length);
    if (!buffer.valid) {
        *length = 0U;
        return false;
    }

    *length = buffer.length;
    return true;
}
