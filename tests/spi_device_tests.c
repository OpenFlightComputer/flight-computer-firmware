#include "spi_device.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool initialize_result;
    bool select_result;
    bool transfer_result;
    uint32_t initialize_count;
    uint32_t select_count;
    uint32_t deselect_count;
    uint32_t transfer_count;
    uint32_t delay_duration_us;
} fake_spi_t;

static bool initialize(spi_device_t *device)
{
    fake_spi_t *fake = device->context;
    fake->initialize_count++;
    return fake->initialize_result;
}

static bool select(spi_device_t *device)
{
    fake_spi_t *fake = device->context;
    fake->select_count++;
    return fake->select_result;
}

static void deselect(spi_device_t *device)
{
    fake_spi_t *fake = device->context;
    fake->deselect_count++;
}

static bool transfer(spi_device_t *device,
                     const uint8_t *transmit,
                     uint8_t *receive,
                     size_t length)
{
    fake_spi_t *fake = device->context;
    (void)transmit;
    (void)receive;
    (void)length;
    fake->transfer_count++;
    return fake->transfer_result;
}

static void delay_us(spi_device_t *device, uint32_t duration_us)
{
    fake_spi_t *fake = device->context;
    fake->delay_duration_us = duration_us;
}

int main(void)
{
    static const spi_device_operations_t operations = {
        .initialize = initialize,
        .select = select,
        .deselect = deselect,
        .transfer = transfer,
        .delay_us = delay_us,
    };
    fake_spi_t fake = {
        .initialize_result = true,
        .select_result = true,
        .transfer_result = true,
    };
    spi_device_t device = {
        .operations = &operations,
        .context = &fake,
    };
    uint8_t transmit = 0U;
    uint8_t receive = 0U;

    assert(!spi_device_initialize(NULL));
    assert(!spi_device_initialize(&(spi_device_t){0}));
    assert(spi_device_initialize(&device));
    assert(spi_device_select(&device));
    assert(spi_device_transfer(&device, &transmit, &receive, 1U));
    spi_device_deselect(&device);
    spi_device_delay_us(&device, 25U);

    assert(fake.initialize_count == 1U);
    assert(fake.select_count == 1U);
    assert(fake.transfer_count == 1U);
    assert(fake.deselect_count == 1U);
    assert(fake.delay_duration_us == 25U);
    return 0;
}
