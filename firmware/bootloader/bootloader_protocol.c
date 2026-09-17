#include "bootloader_protocol.h"

#include "bootloader_contract.h"
#include "bootloader_crc32.h"
#include "bootloader_flash.h"
#include "bootloader_image.h"
#include "usb_cdc_transport.h"

#include "stm32f4xx.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DATA_BYTE_CAPACITY 1024U

typedef struct {
    uint32_t expected_length;
    uint32_t expected_crc32;
    uint32_t received_length;
    bool receiving;
    bool application_valid;
    bool vbus_sensing_enabled;
    bool reset_pending;
} bootloader_protocol_state_t;

static bootloader_protocol_state_t state;
static uint8_t line_buffer[USB_CDC_RECEIVE_LINE_CAPACITY];
static uint8_t data_buffer[DATA_BYTE_CAPACITY];

static void send_text(const char *text)
{
    (void)usb_cdc_transport_try_write((const uint8_t *)text, strlen(text));
}

static bool parse_decimal(const char **cursor, uint32_t *value)
{
    uint32_t result = 0U;
    bool found = false;

    while ((**cursor >= '0') && (**cursor <= '9')) {
        const uint32_t digit = (uint32_t)(**cursor - '0');

        if (result > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        result = result * 10U + digit;
        (*cursor)++;
        found = true;
    }
    *value = result;
    return found;
}

static int hex_value(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return character - '0';
    }
    if ((character >= 'a') && (character <= 'f')) {
        return character - 'a' + 10;
    }
    if ((character >= 'A') && (character <= 'F')) {
        return character - 'A' + 10;
    }
    return -1;
}

static bool parse_hex_u32(const char **cursor, uint32_t *value)
{
    uint32_t result = 0U;
    size_t count = 0U;
    int digit;

    while ((digit = hex_value(**cursor)) >= 0) {
        if (count == 8U) {
            return false;
        }
        result = (result << 4U) | (uint32_t)digit;
        (*cursor)++;
        count++;
    }
    *value = result;
    return count > 0U;
}

static bool parse_hex_bytes(const char *cursor,
                            uint8_t *destination,
                            size_t capacity,
                            size_t *length)
{
    size_t count = 0U;

    while (*cursor != '\0') {
        const int high = hex_value(cursor[0]);
        const int low = hex_value(cursor[1]);

        if ((high < 0) || (low < 0) || (count == capacity)) {
            return false;
        }
        destination[count++] = (uint8_t)((high << 4) | low);
        cursor += 2;
    }
    *length = count;
    return count > 0U;
}

static size_t append_decimal(char *destination, size_t capacity, uint32_t value)
{
    char reverse[10];
    size_t digits = 0U;
    size_t index;

    do {
        reverse[digits++] = (char)('0' + value % 10U);
        value /= 10U;
    } while ((value != 0U) && (digits < sizeof(reverse)));
    if (digits > capacity) {
        return 0U;
    }
    for (index = 0U; index < digits; ++index) {
        destination[index] = reverse[digits - index - 1U];
    }
    return digits;
}

static void send_ack(void)
{
    char response[24] = "ACK ";
    size_t length = 4U;

    length += append_decimal(&response[length],
                             sizeof(response) - length - 2U,
                             state.received_length);
    response[length++] = '\n';
    response[length] = '\0';
    send_text(response);
}

static void handle_info(void)
{
    send_text(state.vbus_sensing_enabled
                  ? "OFCBOOT 1 VBUS_SENSE\n"
                  : "OFCBOOT 1 VBUS_ASSUME\n");
}

static void handle_begin(const char *arguments)
{
    uint32_t length;
    uint32_t crc32;

    if (!parse_decimal(&arguments, &length) || (*arguments++ != ' ') ||
        !parse_hex_u32(&arguments, &crc32) || (*arguments != '\0') ||
        (length < 8U) || (length > OFC_APPLICATION_MAXIMUM_LENGTH)) {
        send_text("ERR BEGIN_ARGUMENTS\n");
        return;
    }
    state.receiving = false;
    if (!bootloader_flash_begin()) {
        send_text("ERR ERASE\n");
        return;
    }
    state.expected_length = length;
    state.expected_crc32 = crc32;
    state.received_length = 0U;
    state.application_valid = false;
    state.receiving = true;
    send_text("READY\n");
}

static void handle_data(const char *arguments)
{
    uint32_t offset;
    size_t length;

    if (!state.receiving || !parse_decimal(&arguments, &offset) ||
        (*arguments++ != ' ') ||
        !parse_hex_bytes(arguments, data_buffer, sizeof(data_buffer), &length) ||
        (offset != state.received_length) ||
        (state.received_length > state.expected_length) ||
        (length > state.expected_length - state.received_length) ||
        (((state.received_length + length) < state.expected_length) &&
         ((length % sizeof(uint32_t)) != 0U)) ||
        !bootloader_flash_write(offset, data_buffer, length)) {
        send_text("ERR DATA\n");
        return;
    }
    state.received_length += (uint32_t)length;
    send_ack();
}

static void handle_end(void)
{
    const uint32_t actual_crc32 =
        bootloader_crc32((const void *)OFC_APPLICATION_FLASH_START,
                         state.expected_length);

    if (!state.receiving ||
        (state.received_length != state.expected_length) ||
        (actual_crc32 != state.expected_crc32)) {
        send_text("ERR VERIFY\n");
        return;
    }
    if (!bootloader_flash_commit(state.expected_length, actual_crc32) ||
        !bootloader_application_is_valid()) {
        send_text("ERR COMMIT\n");
        return;
    }
    state.receiving = false;
    state.application_valid = true;
    send_text("OK\n");
}

static void handle_line(const char *line)
{
    if (strcmp(line, "INFO") == 0) {
        handle_info();
    } else if (strncmp(line, "BEGIN ", 6U) == 0) {
        handle_begin(line + 6U);
    } else if (strncmp(line, "DATA ", 5U) == 0) {
        handle_data(line + 5U);
    } else if (strcmp(line, "END") == 0) {
        handle_end();
    } else if ((strcmp(line, "BOOT") == 0) && state.application_valid) {
        send_text("BOOTING\n");
        state.reset_pending = true;
    } else {
        send_text("ERR COMMAND\n");
    }
}

void bootloader_protocol_initialize(bool application_valid,
                                    bool vbus_sensing_enabled)
{
    state = (bootloader_protocol_state_t){
        .application_valid = application_valid,
        .vbus_sensing_enabled = vbus_sensing_enabled,
    };
}

void bootloader_protocol_process(void)
{
    size_t length = 0U;

    usb_cdc_transport_process();
    if (usb_cdc_transport_read_line(line_buffer,
                                    sizeof(line_buffer),
                                    &length) == USB_CDC_LINE_AVAILABLE) {
        line_buffer[length] = 0U;
        handle_line((const char *)line_buffer);
    }
    usb_cdc_transport_process();
    if (state.reset_pending && (usb_cdc_transport_queued_count() == 0U)) {
        NVIC_SystemReset();
    }
}
