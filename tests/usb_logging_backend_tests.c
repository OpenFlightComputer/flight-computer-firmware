#include "logging.h"
#include "usb_cdc_transport.h"
#include "usb_logging_backend.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static usb_cdc_write_result_t fake_write_result;
static uint8_t captured_line[USB_CDC_TRANSMIT_CAPACITY];
static size_t captured_length;
static size_t write_call_count;

usb_cdc_write_result_t usb_cdc_transport_try_write(const uint8_t *data,
                                                   size_t length)
{
    write_call_count++;
    captured_length = length;
    if (length <= sizeof(captured_line)) {
        memcpy(captured_line, data, length);
    }
    return fake_write_result;
}

static void reset_fake_transport(usb_cdc_write_result_t result)
{
    fake_write_result = result;
    captured_line[0] = 0U;
    captured_length = 0U;
    write_call_count = 0U;
}

static void attach_production_backend(void)
{
    const logging_backend_t backend = usb_logging_backend();

    logging_initialize();
    assert(logging_attach_backend(&backend) == LOGGING_BACKEND_ATTACH_OK);
}

static void accepted_record_is_copied_and_removed(void)
{
    static const char expected[] =
        "[----------] #0000000001 INFO  SYSTEM    boot\n";

    reset_fake_transport(USB_CDC_WRITE_ACCEPTED);
    attach_production_backend();
    assert(logging_write(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "boot") ==
           LOGGING_WRITE_ENQUEUED);

    assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    assert(write_call_count == 1U);
    assert(captured_length == sizeof(expected) - 1U);
    assert(memcmp(captured_line, expected, sizeof(expected) - 1U) == 0);
    assert(logging_queue_count() == 0U);
}

static void busy_transport_retains_record_for_retry(void)
{
    reset_fake_transport(USB_CDC_WRITE_BUSY);
    attach_production_backend();
    assert(logging_write(LOG_LEVEL_WARN, LOG_MODULE_USB, "host busy") ==
           LOGGING_WRITE_ENQUEUED);

    assert(logging_drain_once() == LOGGING_DRAIN_BACKEND_BUSY);
    assert(logging_queue_count() == 1U);
    assert(firmware_logging_system.statistics.backend_busy_count == 1U);

    fake_write_result = USB_CDC_WRITE_ACCEPTED;
    assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    assert(write_call_count == 2U);
    assert(logging_queue_count() == 0U);
}

static void transport_error_drops_record_without_blocking_queue(void)
{
    reset_fake_transport(USB_CDC_WRITE_ERROR);
    attach_production_backend();
    assert(logging_write(LOG_LEVEL_ERROR, LOG_MODULE_USB, "write failed") ==
           LOGGING_WRITE_ENQUEUED);

    assert(logging_drain_once() == LOGGING_DRAIN_BACKEND_ERROR);
    assert(logging_queue_count() == 0U);
    assert(firmware_logging_system.statistics.backend_error_count == 1U);
}

int main(void)
{
    accepted_record_is_copied_and_removed();
    busy_transport_retains_record_for_retry();
    transport_error_drops_record_without_blocking_queue();
    return 0;
}
