#ifndef OPENFLIGHTCOMPUTER_BMI270_DRIVER_H
#define OPENFLIGHTCOMPUTER_BMI270_DRIVER_H

#include "spi_device.h"

#include "bmi2.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t acceleration_x;
    int16_t acceleration_y;
    int16_t acceleration_z;
    int16_t gyroscope_x;
    int16_t gyroscope_y;
    int16_t gyroscope_z;
} bmi270_raw_sample_t;

typedef enum {
    BMI270_DRIVER_INIT_OK = 0,
    BMI270_DRIVER_INIT_INVALID_ARGUMENT,
    BMI270_DRIVER_INIT_SPI_ERROR,
    BMI270_DRIVER_INIT_SENSOR_ERROR,
    BMI270_DRIVER_INIT_CONFIGURATION_ERROR,
    BMI270_DRIVER_INIT_ENABLE_ERROR,
    BMI270_DRIVER_INIT_ALREADY_INITIALIZED,
} bmi270_driver_init_result_t;

typedef enum {
    BMI270_DRIVER_SAMPLE_OK = 0,
    BMI270_DRIVER_SAMPLE_INVALID_ARGUMENT,
    BMI270_DRIVER_SAMPLE_NOT_INITIALIZED,
    BMI270_DRIVER_SAMPLE_COMMUNICATION_ERROR,
} bmi270_driver_sample_result_t;

typedef struct {
    struct bmi2_dev device;
    spi_device_t *spi;
    int8_t last_sensor_result;
    bool initialized;
} bmi270_driver_t;

bmi270_driver_init_result_t bmi270_driver_initialize(
    bmi270_driver_t *driver,
    spi_device_t *spi);
bmi270_driver_sample_result_t bmi270_driver_read_raw(
    bmi270_driver_t *driver,
    bmi270_raw_sample_t *sample);

#endif
