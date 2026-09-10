#include "bmi270_driver.h"

#include "bmi270.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    bool initialize_result;
    bool select_result;
    bool transfer_result;
    uint8_t last_transmit[64];
    size_t last_length;
    uint32_t select_count;
    uint32_t deselect_count;
    uint32_t transfer_count;
    uint32_t delay_us;
} fake_spi_t;

static int8_t sensor_init_result;
static int8_t sensor_config_result;
static int8_t sensor_enable_result;
static int8_t sensor_sample_result;
static bool callbacks_validated;
static bool configuration_validated;
static bool enable_validated;

static bool fake_initialize(spi_device_t *device)
{
    return ((fake_spi_t *)device->context)->initialize_result;
}

static bool fake_select(spi_device_t *device)
{
    fake_spi_t *fake = device->context;
    fake->select_count++;
    return fake->select_result;
}

static void fake_deselect(spi_device_t *device)
{
    ((fake_spi_t *)device->context)->deselect_count++;
}

static bool fake_transfer(spi_device_t *device,
                          const uint8_t *transmit,
                          uint8_t *receive,
                          size_t length)
{
    fake_spi_t *fake = device->context;

    assert(length <= sizeof(fake->last_transmit));
    fake->transfer_count++;
    fake->last_length = length;
    memcpy(fake->last_transmit, transmit, length);
    memset(receive, 0, length);
    if ((transmit[0] & BMI2_SPI_RD_MASK) != 0U) {
        receive[1] = UINT8_C(0xA5);
        if (length > 2U) {
            receive[2] = UINT8_C(0x5A);
        }
    }
    return fake->transfer_result;
}

static void fake_delay(spi_device_t *device, uint32_t duration_us)
{
    ((fake_spi_t *)device->context)->delay_us = duration_us;
}

static const spi_device_operations_t fake_operations = {
    .initialize = fake_initialize,
    .select = fake_select,
    .deselect = fake_deselect,
    .transfer = fake_transfer,
    .delay_us = fake_delay,
};

static void reset_stubs(void)
{
    sensor_init_result = BMI2_OK;
    sensor_config_result = BMI2_OK;
    sensor_enable_result = BMI2_OK;
    sensor_sample_result = BMI2_OK;
    callbacks_validated = false;
    configuration_validated = false;
    enable_validated = false;
}

int8_t bmi270_init(struct bmi2_dev *device)
{
    uint8_t read_data[2] = {0};
    const uint8_t write_data[2] = {UINT8_C(0x12), UINT8_C(0x34)};

    assert(device != NULL);
    assert(device->intf == BMI2_SPI_INTF);
    assert(device->read_write_len == 32U);
    assert(device->config_file_ptr == NULL);
    assert(device->intf_ptr != NULL);
    assert(device->read(UINT8_C(0x12),
                        read_data,
                        sizeof(read_data),
                        device->intf_ptr) == BMI2_OK);
    assert(read_data[0] == UINT8_C(0xA5));
    assert(read_data[1] == UINT8_C(0x5A));
    assert(device->write(UINT8_C(0x92),
                         write_data,
                         sizeof(write_data),
                         device->intf_ptr) == BMI2_OK);
    assert(device->read(UINT8_C(0x12),
                        read_data,
                        64U,
                        device->intf_ptr) == BMI2_E_COM_FAIL);
    device->delay_us(75U, device->intf_ptr);
    callbacks_validated = true;
    return sensor_init_result;
}

int8_t bmi2_set_sensor_config(struct bmi2_sens_config *configuration,
                              uint8_t sensor_count,
                              struct bmi2_dev *device)
{
    assert(device != NULL);
    assert(sensor_count == 2U);
    assert(configuration[0].type == BMI2_ACCEL);
    assert(configuration[0].cfg.acc.odr == BMI2_ACC_ODR_100HZ);
    assert(configuration[0].cfg.acc.range == BMI2_ACC_RANGE_2G);
    assert(configuration[0].cfg.acc.bwp == BMI2_ACC_NORMAL_AVG4);
    assert(configuration[0].cfg.acc.filter_perf == BMI2_PERF_OPT_MODE);
    assert(configuration[1].type == BMI2_GYRO);
    assert(configuration[1].cfg.gyr.odr == BMI2_GYR_ODR_100HZ);
    assert(configuration[1].cfg.gyr.range == BMI2_GYR_RANGE_2000);
    assert(configuration[1].cfg.gyr.bwp == BMI2_GYR_NORMAL_MODE);
    assert(configuration[1].cfg.gyr.noise_perf == BMI2_POWER_OPT_MODE);
    assert(configuration[1].cfg.gyr.filter_perf == BMI2_PERF_OPT_MODE);
    configuration_validated = true;
    return sensor_config_result;
}

int8_t bmi2_sensor_enable(const uint8_t *sensors,
                          uint8_t sensor_count,
                          struct bmi2_dev *device)
{
    assert(device != NULL);
    assert(sensor_count == 2U);
    assert(sensors[0] == BMI2_ACCEL);
    assert(sensors[1] == BMI2_GYRO);
    enable_validated = true;
    return sensor_enable_result;
}

int8_t bmi2_get_sensor_data(struct bmi2_sens_data *data,
                            struct bmi2_dev *device)
{
    assert(data != NULL);
    assert(device != NULL);
    data->acc.x = 1;
    data->acc.y = -2;
    data->acc.z = 3;
    data->gyr.x = -4;
    data->gyr.y = 5;
    data->gyr.z = -6;
    return sensor_sample_result;
}

static spi_device_t make_spi(fake_spi_t *fake)
{
    *fake = (fake_spi_t){
        .initialize_result = true,
        .select_result = true,
        .transfer_result = true,
    };
    return (spi_device_t){
        .operations = &fake_operations,
        .context = fake,
    };
}

static void initializes_with_proven_configuration_and_reads_sample(void)
{
    bmi270_driver_t driver = {0};
    bmi270_raw_sample_t sample;
    fake_spi_t fake;
    spi_device_t spi = make_spi(&fake);

    reset_stubs();
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_OK);
    assert(callbacks_validated);
    assert(configuration_validated);
    assert(enable_validated);
    assert(fake.select_count == 2U);
    assert(fake.deselect_count == 2U);
    assert(fake.transfer_count == 2U);
    assert(fake.delay_us == 75U);
    assert(fake.last_transmit[0] == UINT8_C(0x12));
    assert(fake.last_transmit[1] == UINT8_C(0x12));
    assert(fake.last_transmit[2] == UINT8_C(0x34));
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_ALREADY_INITIALIZED);

    assert(bmi270_driver_read_raw(&driver, &sample) ==
           BMI270_DRIVER_SAMPLE_OK);
    assert(sample.acceleration_x == 1);
    assert(sample.acceleration_y == -2);
    assert(sample.acceleration_z == 3);
    assert(sample.gyroscope_x == -4);
    assert(sample.gyroscope_y == 5);
    assert(sample.gyroscope_z == -6);
}

static void reports_each_initialization_boundary(void)
{
    bmi270_driver_t driver = {0};
    fake_spi_t fake;
    spi_device_t spi = make_spi(&fake);

    reset_stubs();
    assert(bmi270_driver_initialize(NULL, &spi) ==
           BMI270_DRIVER_INIT_INVALID_ARGUMENT);
    assert(bmi270_driver_initialize(&driver, NULL) ==
           BMI270_DRIVER_INIT_INVALID_ARGUMENT);

    fake.initialize_result = false;
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_SPI_ERROR);

    spi = make_spi(&fake);
    sensor_init_result = BMI2_E_DEV_NOT_FOUND;
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_SENSOR_ERROR);

    spi = make_spi(&fake);
    reset_stubs();
    sensor_config_result = BMI2_E_COM_FAIL;
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_CONFIGURATION_ERROR);

    spi = make_spi(&fake);
    reset_stubs();
    sensor_enable_result = BMI2_E_COM_FAIL;
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_ENABLE_ERROR);
}

static void rejects_invalid_and_failed_sample_reads(void)
{
    bmi270_driver_t driver = {0};
    bmi270_raw_sample_t sample;
    fake_spi_t fake;
    spi_device_t spi = make_spi(&fake);

    reset_stubs();
    assert(bmi270_driver_read_raw(NULL, &sample) ==
           BMI270_DRIVER_SAMPLE_INVALID_ARGUMENT);
    assert(bmi270_driver_read_raw(&driver, NULL) ==
           BMI270_DRIVER_SAMPLE_INVALID_ARGUMENT);
    assert(bmi270_driver_read_raw(&driver, &sample) ==
           BMI270_DRIVER_SAMPLE_NOT_INITIALIZED);
    assert(bmi270_driver_initialize(&driver, &spi) ==
           BMI270_DRIVER_INIT_OK);
    sensor_sample_result = BMI2_E_COM_FAIL;
    assert(bmi270_driver_read_raw(&driver, &sample) ==
           BMI270_DRIVER_SAMPLE_COMMUNICATION_ERROR);
}

int main(void)
{
    initializes_with_proven_configuration_and_reads_sample();
    reports_each_initialization_boundary();
    rejects_invalid_and_failed_sample_reads();
    return 0;
}
