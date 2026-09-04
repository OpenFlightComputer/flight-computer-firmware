#ifndef OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_MAP_H
#define OPENFLIGHTCOMPUTER_FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_MAP_H

#include <stddef.h>
#include <stdint.h>

#define FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT 4U
#define FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION 3U
#define FLIGHTCOMPUTER_V1_MOTOR_TIMER_INSTANCE 8U
#define FLIGHTCOMPUTER_V1_MOTOR_DMA_CONTROLLER 2U
#define FLIGHTCOMPUTER_V1_MOTOR_DMA_STREAM 1U
#define FLIGHTCOMPUTER_V1_MOTOR_DMA_CHANNEL 7U
#define FLIGHTCOMPUTER_V1_MOTOR_FIRST_COMPARE_REGISTER 1U
#define FLIGHTCOMPUTER_V1_MOTOR_COMPARE_REGISTER_COUNT 4U

typedef enum {
    FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M1 = 0,
    FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M2,
    FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M3,
    FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M4,
} flightcomputer_v1_physical_output_t;

typedef enum {
    FLIGHTCOMPUTER_V1_GPIO_PORT_C = 2,
} flightcomputer_v1_gpio_port_t;

typedef struct {
    flightcomputer_v1_gpio_port_t gpio_port;
    uint8_t gpio_pin;
    uint8_t gpio_alternate_function;
    uint8_t timer_channel;
} flightcomputer_v1_motor_output_route_t;

typedef struct {
    uint8_t timer_instance;
    uint32_t timer_clock_frequency_hz;
    uint8_t dma_controller;
    uint8_t dma_stream;
    uint8_t dma_channel;
    uint8_t first_compare_register;
    uint8_t compare_register_count;
} flightcomputer_v1_motor_output_group_t;

/* Use a flightcomputer_v1_physical_output_t value as physical_output. */
const flightcomputer_v1_motor_output_route_t *
flightcomputer_v1_motor_output_route(size_t physical_output);

const flightcomputer_v1_motor_output_group_t *
flightcomputer_v1_motor_output_group(void);

#endif
