#include "bmi270_driver.h"

#include "bmi270.h"

#include <stddef.h>
#include <string.h>

#define BMI270_SPI_BUFFER_CAPACITY 64U
#define BMI270_DRIVER_SENSOR_COUNT 2U

static int8_t spi_read(uint8_t register_address,
                       uint8_t *data,
                       uint32_t length,
                       void *context)
{
    bmi270_driver_t *driver = context;
    uint8_t transmit[BMI270_SPI_BUFFER_CAPACITY] = {0};
    uint8_t receive[BMI270_SPI_BUFFER_CAPACITY] = {0};
    bool transfer_ok;

    if ((driver == NULL) || (driver->spi == NULL) || (data == NULL) ||
        (length == 0U) || ((length + 1U) > sizeof(transmit))) {
        return BMI2_E_COM_FAIL;
    }

    /* Bit 7 requests a read; byte zero received during the address is dummy. */
    transmit[0] = register_address | BMI2_SPI_RD_MASK;
    if (!spi_device_select(driver->spi)) {
        return BMI2_E_COM_FAIL;
    }
    transfer_ok = spi_device_transfer(driver->spi,
                                      transmit,
                                      receive,
                                      length + 1U);
    spi_device_deselect(driver->spi);
    if (!transfer_ok) {
        return BMI2_E_COM_FAIL;
    }
    memcpy(data, &receive[1], length);
    return BMI2_OK;
}

static int8_t spi_write(uint8_t register_address,
                        const uint8_t *data,
                        uint32_t length,
                        void *context)
{
    bmi270_driver_t *driver = context;
    uint8_t transmit[BMI270_SPI_BUFFER_CAPACITY] = {0};
    uint8_t receive[BMI270_SPI_BUFFER_CAPACITY] = {0};
    bool transfer_ok;

    if ((driver == NULL) || (driver->spi == NULL) || (data == NULL) ||
        (length == 0U) || ((length + 1U) > sizeof(transmit))) {
        return BMI2_E_COM_FAIL;
    }

    transmit[0] = register_address & (uint8_t)~BMI2_SPI_RD_MASK;
    memcpy(&transmit[1], data, length);
    if (!spi_device_select(driver->spi)) {
        return BMI2_E_COM_FAIL;
    }
    transfer_ok = spi_device_transfer(driver->spi,
                                      transmit,
                                      receive,
                                      length + 1U);
    spi_device_deselect(driver->spi);
    return transfer_ok ? BMI2_OK : BMI2_E_COM_FAIL;
}

static void delay_us(uint32_t duration_us, void *context)
{
    bmi270_driver_t *driver = context;

    if ((driver != NULL) && (driver->spi != NULL)) {
        spi_device_delay_us(driver->spi, duration_us);
    }
}

bmi270_driver_init_result_t bmi270_driver_initialize(
    bmi270_driver_t *driver,
    spi_device_t *spi)
{
    struct bmi2_sens_config configuration[BMI270_DRIVER_SENSOR_COUNT] = {0};
    uint8_t sensors[BMI270_DRIVER_SENSOR_COUNT] = {BMI2_ACCEL, BMI2_GYRO};

    if ((driver == NULL) || (spi == NULL)) {
        return BMI270_DRIVER_INIT_INVALID_ARGUMENT;
    }
    if (driver->initialized) {
        return BMI270_DRIVER_INIT_ALREADY_INITIALIZED;
    }

    *driver = (bmi270_driver_t){0};
    driver->spi = spi;
    if (!spi_device_initialize(spi)) {
        return BMI270_DRIVER_INIT_SPI_ERROR;
    }

    driver->device.intf = BMI2_SPI_INTF;
    driver->device.read = spi_read;
    driver->device.write = spi_write;
    driver->device.delay_us = delay_us;
    driver->device.read_write_len = 32U;
    driver->device.config_file_ptr = NULL;
    driver->device.intf_ptr = driver;

    driver->last_sensor_result = bmi270_init(&driver->device);
    if (driver->last_sensor_result != BMI2_OK) {
        return BMI270_DRIVER_INIT_SENSOR_ERROR;
    }

    configuration[0].type = BMI2_ACCEL;
    configuration[0].cfg.acc.odr = BMI2_ACC_ODR_1600HZ;
    configuration[0].cfg.acc.range = BMI2_ACC_RANGE_2G;
    configuration[0].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    configuration[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    configuration[1].type = BMI2_GYRO;
    configuration[1].cfg.gyr.odr = BMI2_GYR_ODR_1600HZ;
    configuration[1].cfg.gyr.range = BMI2_GYR_RANGE_2000;
    configuration[1].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
    configuration[1].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
    configuration[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

    driver->last_sensor_result =
        bmi2_set_sensor_config(configuration,
                               BMI270_DRIVER_SENSOR_COUNT,
                               &driver->device);
    if (driver->last_sensor_result != BMI2_OK) {
        return BMI270_DRIVER_INIT_CONFIGURATION_ERROR;
    }
    driver->last_sensor_result =
        bmi2_sensor_enable(sensors,
                           BMI270_DRIVER_SENSOR_COUNT,
                           &driver->device);
    if (driver->last_sensor_result != BMI2_OK) {
        return BMI270_DRIVER_INIT_ENABLE_ERROR;
    }

    driver->initialized = true;
    return BMI270_DRIVER_INIT_OK;
}

bmi270_driver_sample_result_t bmi270_driver_read_raw(
    bmi270_driver_t *driver,
    bmi270_raw_sample_t *sample)
{
    struct bmi2_sens_data data = {0};

    if ((driver == NULL) || (sample == NULL)) {
        return BMI270_DRIVER_SAMPLE_INVALID_ARGUMENT;
    }
    if (!driver->initialized) {
        return BMI270_DRIVER_SAMPLE_NOT_INITIALIZED;
    }

    driver->last_sensor_result =
        bmi2_get_sensor_data(&data, &driver->device);
    if (driver->last_sensor_result != BMI2_OK) {
        return BMI270_DRIVER_SAMPLE_COMMUNICATION_ERROR;
    }

    *sample = (bmi270_raw_sample_t){
        .acceleration_x = data.acc.x,
        .acceleration_y = data.acc.y,
        .acceleration_z = data.acc.z,
        .gyroscope_x = data.gyr.x,
        .gyroscope_y = data.gyr.y,
        .gyroscope_z = data.gyr.z,
    };
    return BMI270_DRIVER_SAMPLE_OK;
}
