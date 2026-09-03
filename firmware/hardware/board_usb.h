#ifndef OPENFLIGHTCOMPUTER_BOARD_USB_H
#define OPENFLIGHTCOMPUTER_BOARD_USB_H

#include <stdbool.h>

typedef enum {
    BOARD_USB_VBUS_MODE_ASSUME_PRESENT = 0,
    BOARD_USB_VBUS_MODE_SENSE_INPUT,
} board_usb_vbus_mode_t;

static inline bool board_usb_vbus_sensing_enabled(
    board_usb_vbus_mode_t mode)
{
    return mode == BOARD_USB_VBUS_MODE_SENSE_INPUT;
}

#endif
