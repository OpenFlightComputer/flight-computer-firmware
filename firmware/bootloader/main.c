#include "bootloader_contract.h"
#include "bootloader_image.h"
#include "bootloader_protocol.h"
#include "board_usb.h"
#include "mcu.h"
#include "usb_cdc_transport.h"

#include "stm32f4xx.h"

#include <stdbool.h>

static bool consume_update_request(void)
{
    bool requested;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
    requested = RTC->BKP0R == OFC_BOOTLOADER_REQUEST_MAGIC;
    if (requested) {
        RTC->BKP0R = 0U;
        __DSB();
    }
    return requested;
}

int main(void)
{
    const bool update_requested = consume_update_request();
    const bool application_valid = bootloader_application_is_valid();
    board_usb_vbus_mode_t vbus_mode;

    if (!update_requested && application_valid) {
        bootloader_start_application();
    }
    if (mcu_initialize() != MCU_INIT_OK) {
        for (;;) {
            __NOP();
        }
    }
    board_usb_prepare_reenumeration();
    vbus_mode = board_usb_select_automatic_vbus_mode();
    if (usb_cdc_transport_initialize() != USB_CDC_INIT_OK) {
        for (;;) {
            __NOP();
        }
    }
    bootloader_protocol_initialize(
        application_valid,
        board_usb_vbus_sensing_enabled(vbus_mode));
    for (;;) {
        bootloader_protocol_process();
    }
}
