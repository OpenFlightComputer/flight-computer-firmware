#ifndef OPENFLIGHTCOMPUTER_USB_CDC_TRANSPORT_H
#define OPENFLIGHTCOMPUTER_USB_CDC_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_CDC_TRANSMIT_QUEUE_DEPTH 2U
#define USB_CDC_TRANSMIT_CAPACITY 768U
#define USB_CDC_RAW_RECEIVE_CAPACITY 512U
#define USB_CDC_RECEIVE_LINE_CAPACITY 256U
#define USB_CDC_RECEIVE_LINE_QUEUE_DEPTH 2U
#define USB_CDC_RECEIVE_PROCESS_BUDGET 64U

typedef enum {
    USB_CDC_INIT_OK = 0,
    USB_CDC_INIT_CORE_ERROR,
    USB_CDC_INIT_CLASS_ERROR,
    USB_CDC_INIT_INTERFACE_ERROR,
    USB_CDC_INIT_START_ERROR,
} usb_cdc_init_result_t;

typedef enum {
    USB_CDC_WRITE_ACCEPTED = 0,
    USB_CDC_WRITE_BUSY,
    USB_CDC_WRITE_ERROR,
} usb_cdc_write_result_t;

typedef enum {
    USB_CDC_LINE_UNAVAILABLE = 0,
    USB_CDC_LINE_AVAILABLE,
    USB_CDC_LINE_BUFFER_TOO_SMALL,
    USB_CDC_LINE_INVALID_ARGUMENT,
} usb_cdc_line_result_t;

typedef struct {
    uint32_t queued_count;
    uint32_t completed_count;
    uint32_t busy_count;
    uint32_t invalid_write_count;
    uint32_t transfer_start_error_count;
    uint32_t received_bytes_dropped;
    uint32_t completed_lines_dropped;
    uint32_t oversized_lines_dropped;
} usb_cdc_transport_statistics_t;

usb_cdc_init_result_t usb_cdc_transport_initialize(void);
void usb_cdc_transport_process(void);
usb_cdc_write_result_t usb_cdc_transport_try_write(const uint8_t *data,
                                                   size_t length);
usb_cdc_line_result_t usb_cdc_transport_read_line(uint8_t *destination,
                                                  size_t capacity,
                                                  size_t *length);
usb_cdc_transport_statistics_t usb_cdc_transport_statistics(void);
size_t usb_cdc_transport_queued_count(void);
bool usb_cdc_transport_is_configured(void);

#endif
