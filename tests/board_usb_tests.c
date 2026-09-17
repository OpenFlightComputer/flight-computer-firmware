#include "board_usb.h"

#include <assert.h>

static void modes_have_explicit_behavior(void)
{
    assert(!board_usb_vbus_sensing_enabled(
        BOARD_USB_VBUS_MODE_ASSUME_PRESENT));
    assert(board_usb_vbus_sensing_enabled(BOARD_USB_VBUS_MODE_SENSE_INPUT));
}

int main(void)
{
    modes_have_explicit_behavior();
    return 0;
}
