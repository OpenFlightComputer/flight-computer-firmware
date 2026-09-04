#include "motor_output_map.h"

#include <assert.h>
#include <stddef.h>

static void route_is(size_t physical_output,
                     uint8_t gpio_pin,
                     uint8_t timer_channel)
{
    const flightcomputer_v1_motor_output_route_t *route =
        flightcomputer_v1_motor_output_route(physical_output);

    assert(route != NULL);
    assert(route->gpio_port == FLIGHTCOMPUTER_V1_GPIO_PORT_C);
    assert(route->gpio_pin == gpio_pin);
    assert(route->gpio_alternate_function == 3U);
    assert(route->timer_channel == timer_channel);
}

static void schematic_outputs_have_fixed_routes(void)
{
    assert(FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT == 4U);

    route_is(FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M1, 9U, 4U);
    route_is(FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M2, 8U, 3U);
    route_is(FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M3, 7U, 2U);
    route_is(FLIGHTCOMPUTER_V1_PHYSICAL_OUTPUT_M4, 6U, 1U);

    assert(flightcomputer_v1_motor_output_route(4U) == NULL);
    assert(flightcomputer_v1_motor_output_route(SIZE_MAX) == NULL);
}

static void group_uses_one_tim8_update_dma_burst(void)
{
    const flightcomputer_v1_motor_output_group_t *group =
        flightcomputer_v1_motor_output_group();

    assert(group != NULL);
    assert(group->timer_instance == 8U);
    assert(group->timer_clock_frequency_hz == 168000000U);
    assert(group->dma_controller == 2U);
    assert(group->dma_stream == 1U);
    assert(group->dma_channel == 7U);
    assert(group->first_compare_register == 1U);
    assert(group->compare_register_count == 4U);
}

int main(void)
{
    schematic_outputs_have_fixed_routes();
    group_uses_one_tim8_update_dma_burst();
    return 0;
}
