#include "motor_output_map.h"

#include "board_definition.h"

static const flightcomputer_v1_motor_output_route_t motor_output_routes[] = {
    {FLIGHTCOMPUTER_V1_GPIO_PORT_C,
     9U,
     FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION,
     4U}, /* ESC_M1 */
    {FLIGHTCOMPUTER_V1_GPIO_PORT_C,
     8U,
     FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION,
     3U}, /* ESC_M2 */
    {FLIGHTCOMPUTER_V1_GPIO_PORT_C,
     7U,
     FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION,
     2U}, /* ESC_M3 */
    {FLIGHTCOMPUTER_V1_GPIO_PORT_C,
     6U,
     FLIGHTCOMPUTER_V1_MOTOR_GPIO_ALTERNATE_FUNCTION,
     1U}, /* ESC_M4 */
};

static const flightcomputer_v1_motor_output_group_t motor_output_group = {
    .timer_instance = FLIGHTCOMPUTER_V1_MOTOR_TIMER_INSTANCE,
    .timer_clock_frequency_hz =
        FLIGHTCOMPUTER_V1_MOTOR_TIMER_CLOCK_FREQUENCY_HZ,
    .dma_controller = FLIGHTCOMPUTER_V1_MOTOR_DMA_CONTROLLER,
    .dma_stream = FLIGHTCOMPUTER_V1_MOTOR_DMA_STREAM,
    .dma_channel = FLIGHTCOMPUTER_V1_MOTOR_DMA_CHANNEL,
    .first_compare_register =
        FLIGHTCOMPUTER_V1_MOTOR_FIRST_COMPARE_REGISTER,
    .compare_register_count =
        FLIGHTCOMPUTER_V1_MOTOR_COMPARE_REGISTER_COUNT,
};

_Static_assert(sizeof(motor_output_routes) / sizeof(motor_output_routes[0]) ==
                   FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT,
               "Every V1 motor output requires one physical route");

const flightcomputer_v1_motor_output_route_t *
flightcomputer_v1_motor_output_route(size_t physical_output)
{
    if (physical_output >= FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT) {
        return NULL;
    }

    return &motor_output_routes[physical_output];
}

const flightcomputer_v1_motor_output_group_t *
flightcomputer_v1_motor_output_group(void)
{
    return &motor_output_group;
}
