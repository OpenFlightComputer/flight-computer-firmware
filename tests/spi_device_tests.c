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
    spi_device_async_result_t async_start_result;
    spi_device_async_result_t async_poll_result;
    uint32_t frequency_hz;
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

static spi_device_async_result_t transfer_start(spi_device_t *device,
                                                const uint8_t *transmit,
                                                uint8_t *receive,
                                                size_t length)
{
    fake_spi_t *fake = device->context;
    (void)transmit;
    (void)receive;
    (void)length;
    return fake->async_start_result;
}

static spi_device_async_result_t transfer_poll(spi_device_t *device)
{
    fake_spi_t *fake = device->context;
    return fake->async_poll_result;
}

static bool set_frequency_hz(spi_device_t *device, uint32_t frequency_hz)
{
    fake_spi_t *fake = device->context;
    fake->frequency_hz = frequency_hz;
    return frequency_hz != 0U;
}

int main(void)
{
    static const spi_device_operations_t operations = {
        .initialize = initialize,
        .select = select,
        .deselect = deselect,
        .transfer = transfer,
        .transfer_start = transfer_start,
        .transfer_poll = transfer_poll,
        .set_frequency_hz = set_frequency_hz,
        .delay_us = delay_us,
    };
    fake_spi_t fake = {
        .initialize_result = true,
        .select_result = true,
        .transfer_result = true,
        .async_start_result = SPI_DEVICE_ASYNC_STARTED,
        .async_poll_result = SPI_DEVICE_ASYNC_COMPLETE,
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
    assert(spi_device_transfer_start(&device, &transmit, &receive, 1U) ==
           SPI_DEVICE_ASYNC_STARTED);
    assert(spi_device_transfer_poll(&device) == SPI_DEVICE_ASYNC_COMPLETE);
    assert(spi_device_set_frequency_hz(&device, 21000000U));
    assert(fake.frequency_hz == 21000000U);
    assert(spi_device_transfer_start(NULL, &transmit, &receive, 1U) ==
           SPI_DEVICE_ASYNC_UNSUPPORTED);
    spi_device_deselect(&device);
    spi_device_delay_us(&device, 25U);

    assert(fake.initialize_count == 1U);
    assert(fake.select_count == 1U);
    assert(fake.transfer_count == 1U);
    assert(fake.deselect_count == 1U);
    assert(fake.delay_duration_us == 25U);
    return 0;
}
