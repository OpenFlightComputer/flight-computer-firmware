#include "usb_blackbox_response.h"

#include "uint64_decimal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool valid;
} json_writer_t;

static void append_format(json_writer_t *writer, const char *format, ...)
{
    va_list arguments;
    int written;
    if (!writer->valid || (writer->length >= writer->capacity)) {
        writer->valid = false;
        return;
    }
    va_start(arguments, format);
    written = vsnprintf(writer->data + writer->length,
                        writer->capacity - writer->length,
                        format,
                        arguments);
    va_end(arguments);
    if ((written < 0) ||
        ((size_t)written >= writer->capacity - writer->length)) {
        writer->valid = false;
        return;
    }
    writer->length += (size_t)written;
}

static void append_u64(json_writer_t *writer, uint64_t value)
{
    char text[UINT64_DECIMAL_BUFFER_CAPACITY];
    size_t length;
    if (!uint64_decimal_format(value, 0U, text, sizeof(text), &length) ||
        !writer->valid || (length >= writer->capacity - writer->length)) {
        writer->valid = false;
        return;
    }
    memcpy(writer->data + writer->length, text, length);
    writer->length += length;
    writer->data[writer->length] = '\0';
}

static bool finish(json_writer_t *writer, size_t *length)
{
    if (!writer->valid || (length == NULL)) {
        return false;
    }
    *length = writer->length;
    return true;
}

static void append_common_status(json_writer_t *writer,
                                 const blackbox_t *blackbox)
{
    const uint64_t average_write_time_us =
        blackbox->completed_sector_write_count == 0U
            ? 0U
            : blackbox->total_sector_write_time_us /
                  blackbox->completed_sector_write_count;

    append_format(writer,
                  "\"status\":\"%s\",\"media_present\":%s,"
                  "\"sector_count\":",
                  blackbox_status_name(blackbox->status),
                  blackbox->card != NULL ? "true" : "false");
    append_u64(writer, blackbox->card != NULL
                           ? blackbox->card->sector_count
                           : 0U);
    append_format(writer,
                  ",\"format_version\":%u,\"sample_interval_us\":%lu,"
                  "\"log_count\":%lu,\"queue_depth\":%lu,"
                  "\"queue_capacity\":%u,\"maximum_queue_depth\":%lu,"
                  "\"captured_samples\":",
                  (unsigned int)BLACKBOX_FORMAT_VERSION,
                  (unsigned long)BLACKBOX_SAMPLE_INTERVAL_US,
                  (unsigned long)blackbox_log_count(blackbox),
                  (unsigned long)blackbox->queue_count,
                  (unsigned int)BLACKBOX_QUEUE_CAPACITY,
                  (unsigned long)blackbox->maximum_queue_depth);
    append_u64(writer, blackbox->captured_sample_count);
    append_format(writer, ",\"dropped_samples\":");
    append_u64(writer, blackbox->dropped_sample_count);
    append_format(writer, ",\"completed_sector_writes\":");
    append_u64(writer, blackbox->completed_sector_write_count);
    append_format(writer, ",\"average_sector_write_us\":");
    append_u64(writer, average_write_time_us);
    append_format(writer, ",\"maximum_sector_write_us\":");
    append_u64(writer, blackbox->maximum_sector_write_time_us);
}

bool usb_blackbox_status_response_build(uint32_t request_id,
                                        const blackbox_t *blackbox,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length)
{
    json_writer_t writer = {destination, capacity, 0U,
                            destination != NULL && capacity > 0U &&
                            blackbox != NULL};
    if (!writer.valid) {
        return false;
    }
    destination[0] = '\0';
    append_format(&writer,
                  "{\"type\":\"response\",\"command\":\"storage_status\","
                  "\"request_id\":%lu,\"ok\":true,",
                  (unsigned long)request_id);
    append_common_status(&writer, blackbox);
    append_format(&writer, "}\n");
    return finish(&writer, length);
}

bool usb_blackbox_initialize_response_build(uint32_t request_id,
                                            bool accepted,
                                            const blackbox_t *blackbox,
                                            const char *error,
                                            char *destination,
                                            size_t capacity,
                                            size_t *length)
{
    json_writer_t writer = {destination, capacity, 0U,
                            destination != NULL && capacity > 0U &&
                            blackbox != NULL};
    if (!writer.valid) {
        return false;
    }
    destination[0] = '\0';
    append_format(&writer,
                  "{\"type\":\"response\",\"command\":"
                  "\"storage_initialize\",\"request_id\":%lu,\"ok\":%s,",
                  (unsigned long)request_id, accepted ? "true" : "false");
    append_common_status(&writer, blackbox);
    if (!accepted && error != NULL) {
        append_format(&writer, ",\"error\":\"%s\"", error);
    }
    append_format(&writer, "}\n");
    return finish(&writer, length);
}

bool usb_blackbox_log_list_response_build(uint32_t request_id,
                                          const blackbox_t *blackbox,
                                          char *destination,
                                          size_t capacity,
                                          size_t *length)
{
    json_writer_t writer = {destination, capacity, 0U,
                            destination != NULL && capacity > 0U &&
                            blackbox != NULL};
    size_t index;
    if (!writer.valid) {
        return false;
    }
    destination[0] = '\0';
    append_format(&writer,
                  "{\"type\":\"response\",\"command\":\"flight_log_list\","
                  "\"request_id\":%lu,\"ok\":true,\"logs\":[",
                  (unsigned long)request_id);
    for (index = 0U; index < blackbox_log_count(blackbox); index++) {
        blackbox_log_information_t log;
        if (!blackbox_log_information(blackbox, index, &log)) {
            writer.valid = false;
            break;
        }
        append_format(&writer,
                      "%s{\"id\":%lu,\"start_sector\":%lu,"
                      "\"end_sector\":%lu,\"sector_count\":%lu,"
                      "\"sample_count\":%lu,\"dropped_sample_count\":%lu,"
                      "\"complete\":%s}",
                      index == 0U ? "" : ",",
                      (unsigned long)log.id,
                      (unsigned long)log.start_sector,
                      (unsigned long)log.end_sector,
                      (unsigned long)(log.end_sector - log.start_sector + 1U),
                      (unsigned long)log.sample_count,
                      (unsigned long)log.dropped_sample_count,
                      log.complete ? "true" : "false");
    }
    append_format(&writer, "]}\n");
    return finish(&writer, length);
}

bool usb_blackbox_log_read_response_build(
    uint32_t request_id,
    uint32_t log_id,
    uint32_t sector_offset,
    const uint8_t sector[SD_CARD_SECTOR_SIZE],
    char *destination,
    size_t capacity,
    size_t *length)
{
    static const char hexadecimal[] = "0123456789abcdef";
    json_writer_t writer = {destination, capacity, 0U,
                            destination != NULL && capacity > 0U &&
                            sector != NULL};
    size_t index;
    if (!writer.valid) {
        return false;
    }
    destination[0] = '\0';
    append_format(&writer,
                  "{\"type\":\"response\",\"command\":\"flight_log_read\","
                  "\"request_id\":%lu,\"ok\":true,\"log_id\":%lu,"
                  "\"sector_offset\":%lu,\"data_hex\":\"",
                  (unsigned long)request_id, (unsigned long)log_id,
                  (unsigned long)sector_offset);
    if (writer.valid &&
        ((SD_CARD_SECTOR_SIZE * 2U) < writer.capacity - writer.length)) {
        for (index = 0U; index < SD_CARD_SECTOR_SIZE; index++) {
            destination[writer.length++] = hexadecimal[sector[index] >> 4U];
            destination[writer.length++] = hexadecimal[sector[index] & 0x0FU];
        }
        destination[writer.length] = '\0';
    } else {
        writer.valid = false;
    }
    append_format(&writer, "\"}\n");
    return finish(&writer, length);
}
