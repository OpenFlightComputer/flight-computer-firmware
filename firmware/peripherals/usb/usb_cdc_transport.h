#ifndef OPENFLIGHTCOMPUTER_USB_CDC_TRANSPORT_H
#define OPENFLIGHTCOMPUTER_USB_CDC_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_CDC_TRANSMIT_QUEUE_DEPTH 2U
#define USB_CDC_TRANSMIT_CAPACITY 160U

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

typedef struct {
    uint32_t queued_count;
    uint32_t completed_count;
    uint32_t busy_count;
    uint32_t invalid_write_count;
    uint32_t transfer_start_error_count;
    uint32_t received_bytes_ignored;
} usb_cdc_transport_statistics_t;

usb_cdc_init_result_t usb_cdc_transport_initialize(void);
void usb_cdc_transport_process(void);
usb_cdc_write_result_t usb_cdc_transport_try_write(const uint8_t *data,
                                                   size_t length);
usb_cdc_transport_statistics_t usb_cdc_transport_statistics(void);
size_t usb_cdc_transport_queued_count(void);
bool usb_cdc_transport_is_configured(void);

#endif

