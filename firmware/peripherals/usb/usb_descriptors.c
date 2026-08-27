#include "usb_descriptors.h"

#include "usb_identity.h"
#include "usbd_core.h"

#include <stdint.h>

#define USB_LANGUAGE_ID_ENGLISH_US 0x0409U

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *language_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *configuration_descriptor(
    USBD_SpeedTypeDef speed,
    uint16_t *length
);
static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef openflightcomputer_usb_descriptors = {
    device_descriptor,
    language_descriptor,
    manufacturer_descriptor,
    product_descriptor,
    serial_descriptor,
    configuration_descriptor,
    interface_descriptor,
};

__ALIGN_BEGIN static uint8_t device_descriptor_data[USB_LEN_DEV_DESC]
    __ALIGN_END = {
        USB_LEN_DEV_DESC,
        USB_DESC_TYPE_DEVICE,
        0x00U,
        0x02U,
        0x02U,
        0x02U,
        0x00U,
        USB_MAX_EP0_SIZE,
        LOBYTE(OPENFLIGHTCOMPUTER_USB_VID),
        HIBYTE(OPENFLIGHTCOMPUTER_USB_VID),
        LOBYTE(OPENFLIGHTCOMPUTER_USB_PID),
        HIBYTE(OPENFLIGHTCOMPUTER_USB_PID),
        0x00U,
        0x01U,
        USBD_IDX_MFC_STR,
        USBD_IDX_PRODUCT_STR,
        0x00U,
        USBD_MAX_NUM_CONFIGURATION,
    };

__ALIGN_BEGIN static uint8_t language_descriptor_data[USB_LEN_LANGID_STR_DESC]
    __ALIGN_END = {
        USB_LEN_LANGID_STR_DESC,
        USB_DESC_TYPE_STRING,
        LOBYTE(USB_LANGUAGE_ID_ENGLISH_US),
        HIBYTE(USB_LANGUAGE_ID_ENGLISH_US),
    };

__ALIGN_BEGIN static uint8_t string_descriptor_data[USBD_MAX_STR_DESC_SIZ]
    __ALIGN_END;

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(device_descriptor_data);
    return device_descriptor_data;
}

static uint8_t *language_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(language_descriptor_data);
    return language_descriptor_data;
}

static uint8_t *string_descriptor(const char *text, uint16_t *length)
{
    USBD_GetString((uint8_t *)text, string_descriptor_data, length);
    return string_descriptor_data;
}

static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return string_descriptor(OPENFLIGHTCOMPUTER_USB_MANUFACTURER, length);
}

static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return string_descriptor(OPENFLIGHTCOMPUTER_USB_PRODUCT, length);
}

static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return string_descriptor("Serial deferred to protocol initialization", length);
}

static uint8_t *configuration_descriptor(
    USBD_SpeedTypeDef speed,
    uint16_t *length
)
{
    (void)speed;
    return string_descriptor(OPENFLIGHTCOMPUTER_USB_CONFIGURATION, length);
}

static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return string_descriptor(OPENFLIGHTCOMPUTER_USB_INTERFACE, length);
}
