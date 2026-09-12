#include "usb_control_trace_response.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "uint64_decimal.h"

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool valid;
} json_writer_t;

static bool format_uint64(uint64_t value, char *output, size_t capacity)
{
    size_t length;

    return uint64_decimal_format(value, 0U, output, capacity, &length);
}

static void writer_append(json_writer_t *writer, const char *text)
{
    size_t text_length;

    if ((writer == NULL) || !writer->valid || (text == NULL)) {
        return;
    }

    text_length = strlen(text);
    if ((text_length >= writer->capacity) ||
        (writer->length > (writer->capacity - text_length - 1U))) {
        writer->valid = false;
        return;
    }

    memcpy(&writer->data[writer->length], text, text_length);
    writer->length += text_length;
    writer->data[writer->length] = '\0';
}

static void writer_append_format(json_writer_t *writer, const char *format, ...)
{
    va_list arguments;
    int written;
    size_t remaining;

    if ((writer == NULL) || !writer->valid || (format == NULL)) {
        return;
    }

    remaining = writer->capacity - writer->length;
    va_start(arguments, format);
    written = vsnprintf(&writer->data[writer->length], remaining, format, arguments);
    va_end(arguments);

    if ((written < 0) || ((size_t)written >= remaining)) {
        writer->valid = false;
        return;
    }

    writer->length += (size_t)written;
}

static int32_t diagnostic_milli(float value)
{
    double scaled;

    if (!isfinite(value)) {
        return 0;
    }

    scaled = (double)value * 1000.0;
    if (scaled >= (double)INT32_MAX) {
        return INT32_MAX;
    }
    if (scaled <= (double)INT32_MIN) {
        return INT32_MIN;
    }

    return (int32_t)lround(scaled);
}

static uint32_t record_validity_flags(const control_trace_record_t *record)
{
    uint32_t flags = 0U;

    if (record->receiver_valid) {
        flags |= (1U << 0U);
    }
    if (record->setpoint_valid) {
        flags |= (1U << 1U);
    }
    if (record->attitude_valid) {
        flags |= (1U << 2U);
    }
    if (record->rate_output_valid) {
        flags |= (1U << 3U);
    }
    if (record->mixer_output_valid) {
        flags |= (1U << 4U);
    }

    return flags;
}

static bool format_record(const control_trace_record_t *record,
                          char *output,
                          size_t output_capacity)
{
    char sequence[UINT64_DECIMAL_BUFFER_CAPACITY];
    char timestamp[UINT64_DECIMAL_BUFFER_CAPACITY];
    char imu_sequence[UINT64_DECIMAL_BUFFER_CAPACITY];
    int result;

    if ((record == NULL) || (output == NULL) || (output_capacity == 0U) ||
        !format_uint64(record->sequence, sequence, sizeof(sequence)) ||
        !format_uint64(record->timestamp_us, timestamp, sizeof(timestamp)) ||
        !format_uint64(record->imu_sequence, imu_sequence, sizeof(imu_sequence))) {
        return false;
    }

    result = snprintf(
        output,
        output_capacity,
        "[%s,%s,%s,%lu,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,"
        "[%ld,%ld,%ld,%ld],[%ld,%ld,%ld,%ld],[%ld,%ld],"
        "[%ld,%ld,%ld],[%ld,%ld,%ld],"
        "[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],"
        "[%ld,%ld,%ld,%ld],%ld,%ld,%u]",
        sequence,
        timestamp,
        imu_sequence,
        (unsigned long)record->event_flags,
        (unsigned int)record->system_state,
        (unsigned int)record->control_source,
        (unsigned int)record->failsafe_state,
        (unsigned int)record->failsafe_action,
        (unsigned int)record->imu_freshness,
        (unsigned int)record->control_result,
        (unsigned int)record->rate_result,
        (unsigned long)record_validity_flags(record),
        (unsigned long)record->dt_us,
        (long)diagnostic_milli(record->receiver[0]),
        (long)diagnostic_milli(record->receiver[1]),
        (long)diagnostic_milli(record->receiver[2]),
        (long)diagnostic_milli(record->receiver[3]),
        (long)diagnostic_milli(record->setpoint[0]),
        (long)diagnostic_milli(record->setpoint[1]),
        (long)diagnostic_milli(record->setpoint[2]),
        (long)diagnostic_milli(record->setpoint[3]),
        (long)diagnostic_milli(record->attitude[0]),
        (long)diagnostic_milli(record->attitude[1]),
        (long)diagnostic_milli(record->desired_rate[0]),
        (long)diagnostic_milli(record->desired_rate[1]),
        (long)diagnostic_milli(record->desired_rate[2]),
        (long)diagnostic_milli(record->measured_rate[0]),
        (long)diagnostic_milli(record->measured_rate[1]),
        (long)diagnostic_milli(record->measured_rate[2]),
        (long)diagnostic_milli(record->pid[0].proportional),
        (long)diagnostic_milli(record->pid[0].integral),
        (long)diagnostic_milli(record->pid[0].derivative),
        (long)diagnostic_milli(record->pid[0].total),
        (long)diagnostic_milli(record->pid[1].proportional),
        (long)diagnostic_milli(record->pid[1].integral),
        (long)diagnostic_milli(record->pid[1].derivative),
        (long)diagnostic_milli(record->pid[1].total),
        (long)diagnostic_milli(record->pid[2].proportional),
        (long)diagnostic_milli(record->pid[2].integral),
        (long)diagnostic_milli(record->pid[2].derivative),
        (long)diagnostic_milli(record->pid[2].total),
        (long)diagnostic_milli(record->motor[0]),
        (long)diagnostic_milli(record->motor[1]),
        (long)diagnostic_milli(record->motor[2]),
        (long)diagnostic_milli(record->motor[3]),
        (long)diagnostic_milli(record->mixer_scale),
        (long)diagnostic_milli(record->collective_shift),
        record->mixer_saturated ? 1U : 0U);

    return (result >= 0) && ((size_t)result < output_capacity);
}

bool usb_control_trace_status_response_build(const char *command,
                                             uint32_t request_id,
                                             bool accepted,
                                             system_state_t system_state,
                                             const char *error,
                                             const control_trace_t *trace,
                                             char *response,
                                             size_t response_capacity,
                                             size_t *response_length)
{
    char capture_id[UINT64_DECIMAL_BUFFER_CAPACITY];
    char dropped[UINT64_DECIMAL_BUFFER_CAPACITY];
    json_writer_t writer = {
        .data = response,
        .capacity = response_capacity,
        .length = 0U,
        .valid = true,
    };

    if ((command == NULL) || (trace == NULL) || (response == NULL) ||
        (response_length == NULL) ||
        (response_capacity == 0U) ||
        !format_uint64(trace->capture_id, capture_id, sizeof(capture_id)) ||
        !format_uint64(trace->dropped_record_count, dropped, sizeof(dropped))) {
        return false;
    }

    response[0] = '\0';
    *response_length = 0U;
    writer_append_format(&writer,
                         "{\"type\":\"response\",\"request_id\":%lu,"
                         "\"command\":\"%s\",\"ok\":%s,\"accepted\":%s,"
                         "\"state\":\"%s\",\"capture_id\":%s,"
                         "\"level\":\"%s\",\"capturing\":%s,"
                         "\"pending_records\":%lu,\"dropped_records\":%s",
                         (unsigned long)request_id,
                         command,
                         accepted ? "true" : "false",
                         accepted ? "true" : "false",
                         system_state_name(system_state),
                         capture_id,
                         control_trace_level_name(trace->level),
                         trace->level != CONTROL_TRACE_LEVEL_OFF ? "true" : "false",
                         (unsigned long)control_trace_pending_count(trace),
                         dropped);
    if (!accepted && (error != NULL)) {
        writer_append_format(&writer, ",\"error\":\"%s\"", error);
    }
    writer_append(&writer, "}\n");

    if (writer.valid) {
        *response_length = writer.length;
    }
    return writer.valid;
}

bool usb_control_trace_read_response_build(uint32_t request_id,
                                           const control_trace_t *trace,
                                           const control_trace_record_t *records,
                                           const control_trace_batch_t *batch,
                                           char *response,
                                           size_t response_capacity,
                                           size_t *response_length,
                                           size_t *serialized_record_count)
{
    char capture_id[UINT64_DECIMAL_BUFFER_CAPACITY];
    char first_sequence[UINT64_DECIMAL_BUFFER_CAPACITY];
    char next_sequence[UINT64_DECIMAL_BUFFER_CAPACITY];
    char dropped[UINT64_DECIMAL_BUFFER_CAPACITY];
    json_writer_t writer = {
        .data = response,
        .capacity = response_capacity,
        .length = 0U,
        .valid = true,
    };
    size_t count = 0U;

    if ((trace == NULL) || (records == NULL) || (batch == NULL) ||
        (response == NULL) ||
        (response_capacity == 0U) || (response_length == NULL) ||
        (serialized_record_count == NULL) ||
        !format_uint64(trace->capture_id, capture_id, sizeof(capture_id)) ||
        !format_uint64(batch->first_sequence, first_sequence,
                       sizeof(first_sequence)) ||
        !format_uint64(batch->next_sequence, next_sequence,
                       sizeof(next_sequence)) ||
        !format_uint64(trace->dropped_record_count, dropped, sizeof(dropped))) {
        return false;
    }

    *serialized_record_count = 0U;
    *response_length = 0U;
    response[0] = '\0';
    writer_append_format(&writer,
                         "{\"type\":\"response\",\"request_id\":%lu,"
                         "\"command\":\"control_trace_read\",\"ok\":true,"
                         "\"schema_version\":1,\"capture_id\":%s,"
                         "\"level\":\"%s\",\"capturing\":%s,"
                         "\"first_sequence\":%s,\"next_sequence\":%s,"
                         "\"pending_records\":%lu,\"dropped_records\":%s,"
                         "\"scale\":1000,\"records\":[",
                         (unsigned long)request_id,
                         capture_id,
                         control_trace_level_name(trace->level),
                         trace->level != CONTROL_TRACE_LEVEL_OFF ? "true" : "false",
                         first_sequence,
                         next_sequence,
                         (unsigned long)control_trace_pending_count(trace),
                         dropped);
    if (!writer.valid) {
        return false;
    }

    for (count = 0U; count < batch->record_count; ++count) {
        char record[640];
        size_t record_length;
        size_t required;

        if (!format_record(&records[count], record, sizeof(record))) {
            return false;
        }

        record_length = strlen(record);
        required = record_length + (count > 0U ? 1U : 0U) + sizeof("]}\n");
        if ((required >= writer.capacity) ||
            (writer.length > (writer.capacity - required))) {
            break;
        }

        if (count > 0U) {
            writer_append(&writer, ",");
        }
        writer_append(&writer, record);
    }

    writer_append(&writer, "]}\n");
    if (!writer.valid) {
        return false;
    }

    *serialized_record_count = count;
    *response_length = writer.length;
    return true;
}
