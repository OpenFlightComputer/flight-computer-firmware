#include "dshot_motor_backend.h"

#include "board.h"
#include "motor_command.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define DMA_VALUE_COUNT (DSHOT_DMA_SLOT_COUNT * DSHOT_OUTPUT_COUNT)

typedef struct {
    uint32_t timer_clock_hz;
    board_motor_output_init_result_t initialize_result;
    board_motor_output_submit_result_t submit_result;
    board_motor_output_stop_result_t stop_result;
    board_motor_output_status_t status;
    uint16_t submitted_values[DMA_VALUE_COUNT];
    uint16_t stop_values[DMA_VALUE_COUNT];
    uint16_t initialized_period_ticks;
    uint32_t initialize_count;
    uint32_t submit_count;
    uint32_t stop_count;
} fake_board_t;

static fake_board_t fake_board;

static void reset_fake_board(void)
{
    fake_board = (fake_board_t){
        .timer_clock_hz = UINT32_C(168000000),
        .initialize_result = BOARD_MOTOR_OUTPUT_INIT_OK,
        .submit_result = BOARD_MOTOR_OUTPUT_SUBMIT_ACCEPTED,
        .stop_result = BOARD_MOTOR_OUTPUT_STOP_ACCEPTED,
        .status = BOARD_MOTOR_OUTPUT_STATUS_IDLE,
    };
}

uint32_t board_motor_output_timer_clock_frequency_hz(void)
{
    return fake_board.timer_clock_hz;
}

board_motor_output_init_result_t board_motor_output_initialize(
    uint16_t bit_period_ticks)
{
    fake_board.initialize_count++;
    fake_board.initialized_period_ticks = bit_period_ticks;
    return fake_board.initialize_result;
}

board_motor_output_submit_result_t board_motor_output_submit(
    const uint16_t *compare_values,
    size_t compare_value_count)
{
    size_t index;

    fake_board.submit_count++;
    assert(compare_value_count == DMA_VALUE_COUNT);
    for (index = 0U; index < compare_value_count; index++) {
        fake_board.submitted_values[index] = compare_values[index];
    }
    return fake_board.submit_result;
}

board_motor_output_stop_result_t board_motor_output_force_stop(
    const uint16_t *stop_compare_values,
    size_t compare_value_count)
{
    size_t index;

    fake_board.stop_count++;
    assert(compare_value_count == DMA_VALUE_COUNT);
    for (index = 0U; index < compare_value_count; index++) {
        fake_board.stop_values[index] = stop_compare_values[index];
    }
    return fake_board.stop_result;
}

board_motor_output_status_t board_motor_output_status(void)
{
    return fake_board.status;
}

static uint16_t independently_encode(uint16_t value)
{
    const uint16_t payload = (uint16_t)(value << 1U);
    const uint16_t checksum =
        (uint16_t)((payload ^ (payload >> 4U) ^ (payload >> 8U)) & 0x0FU);

    return (uint16_t)((payload << 4U) | checksum);
}

static void expect_physical_output_frame(
    const uint16_t values[DMA_VALUE_COUNT],
    size_t physical_output,
    uint16_t frame)
{
    size_t bit;

    for (bit = 0U; bit < DSHOT_FRAME_BIT_COUNT; bit++) {
        const uint16_t mask =
            (uint16_t)(UINT16_C(1) <<
                       (DSHOT_FRAME_BIT_COUNT - 1U - bit));
        const uint16_t expected = (frame & mask) != 0U ? 420U : 210U;

        assert(values[(bit * DSHOT_OUTPUT_COUNT) + physical_output] ==
               expected);
    }
    assert(values[(DSHOT_FRAME_BIT_COUNT * DSHOT_OUTPUT_COUNT) +
                  physical_output] == 0U);
    assert(values[((DSHOT_FRAME_BIT_COUNT + 1U) *
                   DSHOT_OUTPUT_COUNT) + physical_output] == 0U);
}

static motor_command_t make_command(void)
{
    const float throttle[MOTOR_COMMAND_MOTOR_COUNT] = {
        0.0f,
        0.25f,
        0.5f,
        1.0f,
    };
    motor_command_t command;

    assert(motor_command_create(&command, throttle, UINT64_C(1234)) ==
           MOTOR_COMMAND_CREATE_OK);
    return command;
}

static void preparation_and_initialization_are_fail_closed(void)
{
    dshot_motor_backend_t backend_state;
    motor_output_backend_t backend;
    motor_output_t output;

    assert(!dshot_motor_backend_prepare(NULL, &backend));
    assert(!dshot_motor_backend_prepare(&backend_state, NULL));

    reset_fake_board();
    assert(dshot_motor_backend_prepare(&backend_state, &backend));
    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    assert(fake_board.initialize_count == 1U);
    assert(fake_board.initialized_period_ticks == 560U);
    assert(fake_board.stop_count == 1U);
    expect_physical_output_frame(fake_board.stop_values, 0U, UINT16_C(0));

    reset_fake_board();
    fake_board.timer_clock_hz = 0U;
    assert(dshot_motor_backend_prepare(&backend_state, &backend));
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_BACKEND_ERROR);
    assert(fake_board.initialize_count == 0U);

    reset_fake_board();
    fake_board.initialize_result = BOARD_MOTOR_OUTPUT_INIT_HARDWARE_ERROR;
    assert(dshot_motor_backend_prepare(&backend_state, &backend));
    assert(motor_output_initialize(&output, &backend) ==
           MOTOR_OUTPUT_INIT_BACKEND_ERROR);
}

static void complete_commands_reach_the_board_in_physical_order(void)
{
    dshot_motor_backend_t backend_state;
    motor_output_backend_t backend;
    motor_output_t output;
    motor_command_t command = make_command();

    reset_fake_board();
    assert(dshot_motor_backend_prepare(&backend_state, &backend));
    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_ACCEPTED);
    assert(fake_board.submit_count == 1U);
    expect_physical_output_frame(fake_board.submitted_values,
                                 0U,
                                 independently_encode(UINT16_C(0)));
    expect_physical_output_frame(fake_board.submitted_values,
                                 1U,
                                 independently_encode(UINT16_C(548)));
    expect_physical_output_frame(fake_board.submitted_values,
                                 2U,
                                 independently_encode(UINT16_C(1048)));
    expect_physical_output_frame(fake_board.submitted_values,
                                 3U,
                                 independently_encode(UINT16_C(2047)));

    command.throttle[3] = 0.0f;
    expect_physical_output_frame(fake_board.submitted_values,
                                 3U,
                                 independently_encode(UINT16_C(2047)));
}

static void busy_error_status_and_stop_results_are_mapped(void)
{
    dshot_motor_backend_t backend_state;
    motor_output_backend_t backend;
    motor_output_t output;
    const motor_command_t command = make_command();

    reset_fake_board();
    assert(dshot_motor_backend_prepare(&backend_state, &backend));
    assert(motor_output_initialize(&output, &backend) == MOTOR_OUTPUT_INIT_OK);
    fake_board.status = BOARD_MOTOR_OUTPUT_STATUS_ACTIVE;
    assert(motor_output_status(&output) == MOTOR_OUTPUT_STATUS_BUSY);
    assert(motor_output_submit(&output, &command) == MOTOR_OUTPUT_SUBMIT_BUSY);
    assert(fake_board.submit_count == 0U);

    fake_board.status = BOARD_MOTOR_OUTPUT_STATUS_ERROR;
    assert(motor_output_status(&output) ==
           MOTOR_OUTPUT_STATUS_BACKEND_ERROR);
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR);

    fake_board.status = BOARD_MOTOR_OUTPUT_STATUS_IDLE;
    fake_board.submit_result = BOARD_MOTOR_OUTPUT_SUBMIT_BUSY;
    assert(motor_output_submit(&output, &command) == MOTOR_OUTPUT_SUBMIT_BUSY);
    fake_board.submit_result = BOARD_MOTOR_OUTPUT_SUBMIT_ERROR;
    assert(motor_output_submit(&output, &command) ==
           MOTOR_OUTPUT_SUBMIT_BACKEND_ERROR);

    fake_board.stop_result = BOARD_MOTOR_OUTPUT_STOP_ERROR;
    assert(motor_output_force_stop(&output) ==
           MOTOR_OUTPUT_STOP_BACKEND_ERROR);
}

int main(void)
{
    preparation_and_initialization_are_fail_closed();
    complete_commands_reach_the_board_in_physical_order();
    busy_error_status_and_stop_results_are_mapped();
    return 0;
}
