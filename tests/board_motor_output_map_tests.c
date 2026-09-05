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

static void physical_compare_rows_are_reordered_inside_the_board(void)
{
    const uint16_t physical_order[FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT] = {
        UINT16_C(11), UINT16_C(22), UINT16_C(33), UINT16_C(44)};
    uint16_t timer_order[FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT] = {0U};
    uint16_t in_place[FLIGHTCOMPUTER_V1_MOTOR_OUTPUT_COUNT] = {
        UINT16_C(11), UINT16_C(22), UINT16_C(33), UINT16_C(44)};

    assert(flightcomputer_v1_motor_output_order_compare_row(
        physical_order, timer_order));
    assert(timer_order[0] == UINT16_C(44));
    assert(timer_order[1] == UINT16_C(33));
    assert(timer_order[2] == UINT16_C(22));
    assert(timer_order[3] == UINT16_C(11));

    assert(flightcomputer_v1_motor_output_order_compare_row(
        in_place, in_place));
    assert(in_place[0] == UINT16_C(44));
    assert(in_place[1] == UINT16_C(33));
    assert(in_place[2] == UINT16_C(22));
    assert(in_place[3] == UINT16_C(11));

    assert(!flightcomputer_v1_motor_output_order_compare_row(
        NULL, timer_order));
    assert(!flightcomputer_v1_motor_output_order_compare_row(
        physical_order, NULL));
}

int main(void)
{
    schematic_outputs_have_fixed_routes();
    group_uses_one_tim8_update_dma_burst();
    physical_compare_rows_are_reordered_inside_the_board();
    return 0;
}
