#include "board_definition.h"
#include "board_usb.h"

#include <assert.h>

static void modes_have_explicit_behavior(void)
{
    assert(!board_usb_vbus_sensing_enabled(
        BOARD_USB_VBUS_MODE_ASSUME_PRESENT));
    assert(board_usb_vbus_sensing_enabled(BOARD_USB_VBUS_MODE_SENSE_INPUT));
}

static void flightcomputer_v1_assumes_vbus_is_present(void)
{
    assert(FLIGHTCOMPUTER_V1_USB_VBUS_MODE ==
           BOARD_USB_VBUS_MODE_ASSUME_PRESENT);
    assert(!board_usb_vbus_sensing_enabled(
        FLIGHTCOMPUTER_V1_USB_VBUS_MODE));
}

int main(void)
{
    modes_have_explicit_behavior();
    flightcomputer_v1_assumes_vbus_is_present();
    return 0;
}
