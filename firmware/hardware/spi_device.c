#include "spi_device.h"

#include <stddef.h>

bool spi_device_initialize(spi_device_t *device)
{
    return (device != NULL) && (device->operations != NULL) &&
           (device->operations->initialize != NULL) &&
           device->operations->initialize(device);
}

bool spi_device_select(spi_device_t *device)
{
    return (device != NULL) && (device->operations != NULL) &&
           (device->operations->select != NULL) &&
           device->operations->select(device);
}

void spi_device_deselect(spi_device_t *device)
{
    if ((device != NULL) && (device->operations != NULL) &&
        (device->operations->deselect != NULL)) {
        device->operations->deselect(device);
    }
}

bool spi_device_transfer(spi_device_t *device,
                         const uint8_t *transmit,
                         uint8_t *receive,
                         size_t length)
{
    return (device != NULL) && (device->operations != NULL) &&
           (device->operations->transfer != NULL) &&
           device->operations->transfer(device, transmit, receive, length);
}

void spi_device_delay_us(spi_device_t *device, uint32_t duration_us)
{
    if ((device != NULL) && (device->operations != NULL) &&
        (device->operations->delay_us != NULL)) {
        device->operations->delay_us(device, duration_us);
    }
}
