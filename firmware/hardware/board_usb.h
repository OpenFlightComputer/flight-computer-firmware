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

/* Selects sensing only when PA9 observes a valid VBUS logic level. */
board_usb_vbus_mode_t board_usb_select_automatic_vbus_mode(void);
board_usb_vbus_mode_t board_usb_selected_vbus_mode(void);

/* Holds both USB data lines low long enough for the host to observe detach. */
void board_usb_prepare_reenumeration(void);

#endif
