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
        if (length < sizeof(captured_line)) {
            captured_line[length] = 0U;
        }
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
        "{\"type\":\"log\",\"timestamp_us\":null,\"sequence\":1,"
        "\"level\":\"INFO\",\"module\":\"SYSTEM\",\"message\":"
        "\"boot\",\"truncated\":false}\n";

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

static uint64_t fake_clock(void)
{
    return UINT64_C(42);
}

static uint64_t maximum_clock(void)
{
    return UINT64_MAX;
}

static void json_special_characters_are_escaped(void)
{
    static const char message[] = {'\"', '\\', '\n', '\t',
                                   (char)0x01, (char)0x80, '\0'};
    static const char expected[] =
        "{\"type\":\"log\",\"timestamp_us\":42,\"sequence\":1,"
        "\"level\":\"DEBUG\",\"module\":\"USB\",\"message\":"
        "\"\\\"\\\\\\n\\t\\u0001\\u0080\",\"truncated\":false}\n";

    reset_fake_transport(USB_CDC_WRITE_ACCEPTED);
    attach_production_backend();
    assert(logging_attach_clock(fake_clock) == LOGGING_CLOCK_ATTACH_OK);
    assert(logging_write(LOG_LEVEL_DEBUG, LOG_MODULE_USB, "%s", message) ==
           LOGGING_WRITE_FILTERED);
    assert(logging_set_module_threshold(LOG_MODULE_USB,
                                        LOG_THRESHOLD_DEBUG) ==
           LOGGING_CONFIG_OK);
    assert(logging_write(LOG_LEVEL_DEBUG, LOG_MODULE_USB, "%s", message) ==
           LOGGING_WRITE_ENQUEUED);

    assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    assert(captured_length == sizeof(expected) - 1U);
    assert(memcmp(captured_line, expected, sizeof(expected) - 1U) == 0);
}

static void worst_case_message_fits_one_transport_entry(void)
{
    char message[LOGGING_MESSAGE_CAPACITY + 1U];
    size_t index;

    for (index = 0U; index < LOGGING_MESSAGE_CAPACITY; index++) {
        message[index] = (char)0x80;
    }
    message[LOGGING_MESSAGE_CAPACITY] = '\0';

    reset_fake_transport(USB_CDC_WRITE_ACCEPTED);
    attach_production_backend();
    assert(logging_attach_clock(maximum_clock) == LOGGING_CLOCK_ATTACH_OK);
    firmware_logging_system.next_sequence = UINT64_MAX;
    assert(logging_write(LOG_LEVEL_FATAL,
                         LOG_MODULE_SCHEDULER,
                         "%s",
                         message) == LOGGING_WRITE_ENQUEUED);
    assert(logging_peek()->truncated);
    assert(logging_drain_once() == LOGGING_DRAIN_ACCEPTED);
    assert(captured_length < USB_CDC_TRANSMIT_CAPACITY);
    assert(strstr((const char *)captured_line,
                  "\"timestamp_us\":18446744073709551615") != NULL);
    assert(strstr((const char *)captured_line,
                  "\"sequence\":18446744073709551615") != NULL);
    assert(strstr((const char *)captured_line,
                  "\"truncated\":true}\n") != NULL);
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
    json_special_characters_are_escaped();
    worst_case_message_fits_one_transport_entry();
    busy_transport_retains_record_for_retry();
    transport_error_drops_record_without_blocking_queue();
    return 0;
}
