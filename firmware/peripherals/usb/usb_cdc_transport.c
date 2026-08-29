#include "usb_cdc_transport.h"

#include "newline_framer.h"
#include "usb_descriptors.h"
#include "usbd_cdc.h"
#include "usbd_core.h"

#include "stm32f4xx.h"

#include <limits.h>
#include <string.h>

#define USB_CDC_RECEIVE_PACKET_SIZE CDC_DATA_FS_OUT_PACKET_SIZE

typedef struct {
    uint8_t data[USB_CDC_TRANSMIT_CAPACITY];
    size_t length;
} transmit_entry_t;

typedef struct {
    uint8_t data[USB_CDC_RECEIVE_LINE_CAPACITY];
    size_t length;
} received_line_t;

_Static_assert(USB_CDC_RECEIVE_LINE_CAPACITY ==
                   NEWLINE_FRAMER_MAX_LINE_LENGTH,
               "Transport and newline framer line capacities must match");

static int8_t cdc_initialize(void);
static int8_t cdc_deinitialize(void);
static int8_t cdc_control(uint8_t command, uint8_t *buffer, uint16_t length);
static int8_t cdc_receive(uint8_t *buffer, uint32_t *length);
static int8_t cdc_transmit_complete(uint8_t *buffer,
                                    uint32_t *length,
                                    uint8_t endpoint);

static USBD_HandleTypeDef usb_device;
static USBD_CDC_ItfTypeDef cdc_interface = {
    cdc_initialize,
    cdc_deinitialize,
    cdc_control,
    cdc_receive,
    cdc_transmit_complete,
};

static USBD_CDC_LineCodingTypeDef line_coding = {
    115200U,
    0U,
    0U,
    8U,
};

static uint8_t receive_packet[USB_CDC_RECEIVE_PACKET_SIZE];
static uint8_t raw_receive_buffer[USB_CDC_RAW_RECEIVE_CAPACITY];
static volatile uint16_t raw_receive_head;
static volatile uint16_t raw_receive_tail;
static volatile bool receive_overflow_pending;
static newline_framer_t receive_framer;
static received_line_t received_lines[USB_CDC_RECEIVE_LINE_QUEUE_DEPTH];
static size_t received_line_head;
static size_t received_line_tail;
static size_t received_line_count;
static transmit_entry_t transmit_queue[USB_CDC_TRANSMIT_QUEUE_DEPTH];
static size_t transmit_head;
static size_t transmit_tail;
static size_t transmit_count;
static volatile bool transmit_active;
static volatile bool transmit_completed;
static volatile uint32_t received_bytes_dropped;
static bool initialized;
static usb_cdc_transport_statistics_t statistics;

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static void saturating_increment_volatile(volatile uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

static uint32_t enter_critical_section(void)
{
    const uint32_t previous_mask = __get_PRIMASK();

    __disable_irq();
    return previous_mask;
}

static void leave_critical_section(uint32_t previous_mask)
{
    if (previous_mask == 0U) {
        __enable_irq();
    }
}

static void reset_transport_state(void)
{
    usb_device = (USBD_HandleTypeDef){0};
    raw_receive_head = 0U;
    raw_receive_tail = 0U;
    receive_overflow_pending = false;
    newline_framer_initialize(&receive_framer);
    received_lines[0] = (received_line_t){0};
    received_lines[1] = (received_line_t){0};
    received_line_head = 0U;
    received_line_tail = 0U;
    received_line_count = 0U;
    transmit_queue[0] = (transmit_entry_t){0};
    transmit_queue[1] = (transmit_entry_t){0};
    transmit_head = 0U;
    transmit_tail = 0U;
    transmit_count = 0U;
    transmit_active = false;
    transmit_completed = false;
    received_bytes_dropped = 0U;
    initialized = false;
    statistics = (usb_cdc_transport_statistics_t){0};
}

usb_cdc_init_result_t usb_cdc_transport_initialize(void)
{
    reset_transport_state();

    if (USBD_Init(&usb_device, &openflightcomputer_usb_descriptors, 0U) !=
        USBD_OK) {
        return USB_CDC_INIT_CORE_ERROR;
    }
    if (USBD_RegisterClass(&usb_device, USBD_CDC_CLASS) != USBD_OK) {
        (void)USBD_DeInit(&usb_device);
        return USB_CDC_INIT_CLASS_ERROR;
    }
    if (USBD_CDC_RegisterInterface(&usb_device, &cdc_interface) != USBD_OK) {
        (void)USBD_DeInit(&usb_device);
        return USB_CDC_INIT_INTERFACE_ERROR;
    }
    if (USBD_Start(&usb_device) != USBD_OK) {
        (void)USBD_DeInit(&usb_device);
        return USB_CDC_INIT_START_ERROR;
    }

    initialized = true;
    return USB_CDC_INIT_OK;
}

static void completed_line(const uint8_t *line,
                           size_t length,
                           void *context)
{
    received_line_t *destination;

    (void)context;

    if (received_line_count == USB_CDC_RECEIVE_LINE_QUEUE_DEPTH) {
        saturating_increment(&statistics.completed_lines_dropped);
        return;
    }

    destination = &received_lines[received_line_head];
    if (length > 0U) {
        memcpy(destination->data, line, length);
    }
    destination->length = length;
    received_line_head =
        (received_line_head + 1U) % USB_CDC_RECEIVE_LINE_QUEUE_DEPTH;
    received_line_count++;
}

static bool pop_received_byte(uint8_t *byte, bool *overflowed)
{
    const uint32_t previous_mask = enter_critical_section();
    const uint16_t tail = raw_receive_tail;

    *overflowed = receive_overflow_pending;
    receive_overflow_pending = false;

    if (tail == raw_receive_head) {
        leave_critical_section(previous_mask);
        return false;
    }

    *byte = raw_receive_buffer[tail];
    raw_receive_tail =
        (uint16_t)((tail + 1U) % USB_CDC_RAW_RECEIVE_CAPACITY);
    leave_critical_section(previous_mask);
    return true;
}

static void process_received_bytes(void)
{
    uint8_t byte;
    bool overflowed = false;
    size_t processed = 0U;

    while ((processed < USB_CDC_RECEIVE_PROCESS_BUDGET) &&
           pop_received_byte(&byte, &overflowed)) {
        if (overflowed) {
            newline_framer_discard_current_line(&receive_framer);
        }
        newline_framer_consume(&receive_framer,
                               &byte,
                               1U,
                               completed_line,
                               NULL);
        processed++;
    }

    if (overflowed && (processed == 0U)) {
        newline_framer_discard_current_line(&receive_framer);
    }
}

static void finish_completed_transmission(void)
{
    uint32_t previous_mask;
    bool completed;

    previous_mask = enter_critical_section();
    completed = transmit_completed;
    transmit_completed = false;
    leave_critical_section(previous_mask);

    if (!completed || !transmit_active) {
        return;
    }

    transmit_active = false;
    transmit_queue[transmit_tail] = (transmit_entry_t){0};
    transmit_tail =
        (transmit_tail + 1U) % USB_CDC_TRANSMIT_QUEUE_DEPTH;
    transmit_count--;
    saturating_increment(&statistics.completed_count);
}

static void start_pending_transmission(void)
{
    transmit_entry_t *entry;

    if (transmit_active || (transmit_count == 0U) ||
        (usb_device.dev_state != USBD_STATE_CONFIGURED)) {
        return;
    }

    entry = &transmit_queue[transmit_tail];
    transmit_active = true;

    if ((USBD_CDC_SetTxBuffer(&usb_device,
                              entry->data,
                              entry->length) != USBD_OK) ||
        (USBD_CDC_TransmitPacket(&usb_device) != USBD_OK)) {
        transmit_active = false;
        saturating_increment(&statistics.transfer_start_error_count);
    }
}

void usb_cdc_transport_process(void)
{
    if (!initialized) {
        return;
    }

    process_received_bytes();
    finish_completed_transmission();
    start_pending_transmission();
}

usb_cdc_write_result_t usb_cdc_transport_try_write(const uint8_t *data,
                                                   size_t length)
{
    transmit_entry_t *entry;

    if (!initialized || (data == NULL) || (length == 0U) ||
        (length > USB_CDC_TRANSMIT_CAPACITY)) {
        saturating_increment(&statistics.invalid_write_count);
        return USB_CDC_WRITE_ERROR;
    }
    if (transmit_count == USB_CDC_TRANSMIT_QUEUE_DEPTH) {
        saturating_increment(&statistics.busy_count);
        return USB_CDC_WRITE_BUSY;
    }

    entry = &transmit_queue[transmit_head];
    memcpy(entry->data, data, length);
    entry->length = length;
    transmit_head =
        (transmit_head + 1U) % USB_CDC_TRANSMIT_QUEUE_DEPTH;
    transmit_count++;
    saturating_increment(&statistics.queued_count);
    return USB_CDC_WRITE_ACCEPTED;
}

usb_cdc_line_result_t usb_cdc_transport_read_line(uint8_t *destination,
                                                  size_t capacity,
                                                  size_t *length)
{
    received_line_t *line;

    if ((destination == NULL) || (capacity == 0U) || (length == NULL)) {
        return USB_CDC_LINE_INVALID_ARGUMENT;
    }
    if (received_line_count == 0U) {
        *length = 0U;
        return USB_CDC_LINE_UNAVAILABLE;
    }

    line = &received_lines[received_line_tail];
    if (capacity <= line->length) {
        *length = 0U;
        return USB_CDC_LINE_BUFFER_TOO_SMALL;
    }

    if (line->length > 0U) {
        memcpy(destination, line->data, line->length);
    }
    destination[line->length] = 0U;
    *length = line->length;
    *line = (received_line_t){0};
    received_line_tail =
        (received_line_tail + 1U) % USB_CDC_RECEIVE_LINE_QUEUE_DEPTH;
    received_line_count--;
    return USB_CDC_LINE_AVAILABLE;
}

usb_cdc_transport_statistics_t usb_cdc_transport_statistics(void)
{
    usb_cdc_transport_statistics_t snapshot;
    const uint32_t previous_mask = enter_critical_section();

    snapshot = statistics;
    snapshot.received_bytes_dropped = received_bytes_dropped;
    snapshot.oversized_lines_dropped = receive_framer.overflow_count;
    leave_critical_section(previous_mask);
    return snapshot;
}

size_t usb_cdc_transport_queued_count(void)
{
    return transmit_count;
}

bool usb_cdc_transport_is_configured(void)
{
    return initialized && (usb_device.dev_state == USBD_STATE_CONFIGURED);
}

static int8_t cdc_initialize(void)
{
    transmit_active = false;
    transmit_completed = false;
    if ((USBD_CDC_SetTxBuffer(&usb_device, receive_packet, 0U) != USBD_OK) ||
        (USBD_CDC_SetRxBuffer(&usb_device, receive_packet) != USBD_OK) ||
        (USBD_CDC_ReceivePacket(&usb_device) != USBD_OK)) {
        return (int8_t)USBD_FAIL;
    }

    return (int8_t)USBD_OK;
}

static int8_t cdc_deinitialize(void)
{
    transmit_active = false;
    transmit_completed = false;
    return (int8_t)USBD_OK;
}

static int8_t cdc_control(uint8_t command, uint8_t *buffer, uint16_t length)
{
    if ((command == CDC_SET_LINE_CODING) && (length >= 7U)) {
        line_coding.bitrate =
            (uint32_t)buffer[0] |
            ((uint32_t)buffer[1] << 8U) |
            ((uint32_t)buffer[2] << 16U) |
            ((uint32_t)buffer[3] << 24U);
        line_coding.format = buffer[4];
        line_coding.paritytype = buffer[5];
        line_coding.datatype = buffer[6];
    } else if ((command == CDC_GET_LINE_CODING) && (length >= 7U)) {
        buffer[0] = (uint8_t)line_coding.bitrate;
        buffer[1] = (uint8_t)(line_coding.bitrate >> 8U);
        buffer[2] = (uint8_t)(line_coding.bitrate >> 16U);
        buffer[3] = (uint8_t)(line_coding.bitrate >> 24U);
        buffer[4] = line_coding.format;
        buffer[5] = line_coding.paritytype;
        buffer[6] = line_coding.datatype;
    }

    return (int8_t)USBD_OK;
}

static int8_t cdc_receive(uint8_t *buffer, uint32_t *length)
{
    uint32_t index;

    for (index = 0U; index < *length; index++) {
        const uint16_t head = raw_receive_head;
        const uint16_t next =
            (uint16_t)((head + 1U) % USB_CDC_RAW_RECEIVE_CAPACITY);

        if (next == raw_receive_tail) {
            saturating_increment_volatile(&received_bytes_dropped);
            receive_overflow_pending = true;
            continue;
        }

        raw_receive_buffer[head] = buffer[index];
        raw_receive_head = next;
    }

    if ((USBD_CDC_SetRxBuffer(&usb_device, receive_packet) != USBD_OK) ||
        (USBD_CDC_ReceivePacket(&usb_device) != USBD_OK)) {
        return (int8_t)USBD_FAIL;
    }

    return (int8_t)USBD_OK;
}

static int8_t cdc_transmit_complete(uint8_t *buffer,
                                    uint32_t *length,
                                    uint8_t endpoint)
{
    (void)buffer;
    (void)length;
    (void)endpoint;

    transmit_completed = true;
    return (int8_t)USBD_OK;
}
