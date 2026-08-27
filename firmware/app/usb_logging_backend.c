#include "usb_logging_backend.h"

#include "usb_cdc_transport.h"

_Static_assert(USB_CDC_TRANSMIT_CAPACITY >= LOGGING_FORMATTED_LINE_CAPACITY,
               "USB transmit entries must hold one formatted log record");

static logging_backend_result_t try_write_record(const log_record_t *record,
                                                 void *context)
{
    char line[LOGGING_FORMATTED_LINE_CAPACITY];
    size_t line_length;
    logging_format_result_t format_result;
    usb_cdc_write_result_t write_result;

    (void)context;

    format_result = logging_format_record(record,
                                          line,
                                          sizeof(line),
                                          &line_length);
    if (format_result != LOGGING_FORMAT_OK) {
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
