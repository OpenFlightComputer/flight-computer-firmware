#include "usb_logging_backend.h"

#include "logging_config.h"
#include "uint64_decimal.h"
#include "usb_cdc_transport.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool valid;
} json_buffer_t;

_Static_assert(USB_CDC_TRANSMIT_CAPACITY >=
                   (LOGGING_MESSAGE_CAPACITY * 6U + 192U),
               "USB transmit entries must hold worst-case escaped log JSON");

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

static void append_literal(json_buffer_t *buffer, const char *text)
{
    append_bytes(buffer, text, strlen(text));
}

static void append_uint64(json_buffer_t *buffer, uint64_t value)
{
    char decimal[UINT64_DECIMAL_BUFFER_CAPACITY];
    size_t length;

    if (!uint64_decimal_format(value,
                               0U,
                               decimal,
                               sizeof(decimal),
                               &length)) {
        buffer->valid = false;
        return;
    }
    append_bytes(buffer, decimal, length);
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

static void append_json_string_byte(json_buffer_t *buffer, uint8_t byte)
{
    static const char hexadecimal[] = "0123456789abcdef";
    char escape[6] = {'\\', 'u', '0', '0', '0', '0'};

    switch (byte) {
    case (uint8_t)'"':
        append_literal(buffer, "\\\"");
        return;
    case (uint8_t)'\\':
        append_literal(buffer, "\\\\");
        return;
    case (uint8_t)'\b':
        append_literal(buffer, "\\b");
        return;
    case (uint8_t)'\f':
        append_literal(buffer, "\\f");
        return;
    case (uint8_t)'\n':
        append_literal(buffer, "\\n");
        return;
    case (uint8_t)'\r':
        append_literal(buffer, "\\r");
        return;
    case (uint8_t)'\t':
        append_literal(buffer, "\\t");
        return;
    default:
        break;
    }

    if ((byte < UINT8_C(0x20)) || (byte >= UINT8_C(0x7f))) {
        escape[4] = hexadecimal[(byte >> 4U) & 0x0fU];
        escape[5] = hexadecimal[byte & 0x0fU];
        append_bytes(buffer, escape, sizeof(escape));
        return;
    }

    append_bytes(buffer, (const char *)&byte, 1U);
}

static bool format_record_as_json(const log_record_t *record,
                                  char *line,
                                  size_t capacity,
                                  size_t *length)
{
    json_buffer_t buffer = {
        .data = line,
        .capacity = capacity,
        .valid = (line != NULL) && (capacity > 0U),
    };
    size_t index;

    if ((record == NULL) || (line == NULL) || (capacity == 0U) ||
        (length == NULL) ||
        (record->message_length >= LOGGING_MESSAGE_CAPACITY) ||
        (record->message[record->message_length] != '\0')) {
        return false;
    }

    line[0] = '\0';
    append_literal(&buffer, "{\"type\":\"log\",\"timestamp_us\":");
    if (record->timestamp_valid) {
        append_uint64(&buffer, record->timestamp_us);
    } else {
        append_literal(&buffer, "null");
    }
    append_literal(&buffer, ",\"sequence\":");
    append_uint64(&buffer, record->sequence);
    append_format(&buffer,
                  ",\"level\":\"%s\",\"module\":\"%s\",\"message\":\"",
                  logging_level_name(record->level),
                  logging_module_name(record->module));

    for (index = 0U; index < record->message_length; index++) {
        append_json_string_byte(&buffer, (uint8_t)record->message[index]);
    }

    append_format(&buffer,
                  "\",\"truncated\":%s}\n",
                  record->truncated ? "true" : "false");
    if (!buffer.valid) {
        *length = 0U;
        return false;
    }

    *length = buffer.length;
    return true;
}

static logging_backend_result_t try_write_record(const log_record_t *record,
                                                 void *context)
{
    char line[USB_CDC_TRANSMIT_CAPACITY];
    size_t line_length;
    usb_cdc_write_result_t write_result;

    (void)context;

    if (!format_record_as_json(record,
                               line,
                               sizeof(line),
                               &line_length)) {
        return LOG_BACKEND_ERROR;
    }

    write_result = usb_cdc_transport_try_write((const uint8_t *)line,
                                               line_length);
    switch (write_result) {
    case USB_CDC_WRITE_ACCEPTED:
        return LOG_BACKEND_ACCEPTED;
    case USB_CDC_WRITE_BUSY:
        return LOG_BACKEND_BUSY;
    case USB_CDC_WRITE_ERROR:
        return LOG_BACKEND_ERROR;
    }

    return LOG_BACKEND_ERROR;
}

logging_backend_t usb_logging_backend(void)
{
    return (logging_backend_t){
        .try_write = try_write_record,
        .context = NULL,
    };
}
