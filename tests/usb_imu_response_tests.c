#include "usb_imu_response.h"

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void assert_valid_json_line(const char *line, size_t length)
{
    jsmn_parser parser;
    jsmntok_t tokens[160];

    assert(length > 1U);
    assert(line[length - 1U] == '\n');
    jsmn_init(&parser);
    assert(jsmn_parse(&parser, line, length - 1U, tokens, 160U) > 0);
}

static void available_snapshot_is_complete(void)
{
    const usb_imu_diagnostics_t diagnostics = {
        .state = {
            .snapshot = {
                .acceleration_x = 1,
                .acceleration_y = -2,
                .acceleration_z = 16384,
                .gyroscope_x = 3,
                .gyroscope_y = -4,
                .gyroscope_z = 5,
                .sequence = UINT64_MAX,
                .valid = true,
            },
            .freshness = IMU_FRESHNESS_FRESH,
            .age_us = UINT64_MAX,
        },
        .service_statistics = {
            .read_count = 11U,
            .published_sample_count = 10U,
            .source_error_count = 1U,
        },
        .task_execution_count = 12U,
        .task_last_execution_us = 13U,
        .task_maximum_execution_us = 14U,
        .task_overrun_count = 15U,
        .task_missed_release_count = 16U,
        .high_rate_budget_us = 17U,
        .high_rate_utilization_permille = 18U,
        .task_present = true,
        .calibration_state = GYRO_CALIBRATION_READY,
        .calibration_bias = {2, -3, 1},
        .corrected_gyroscope = {1, -1, 4},
        .calibration_sample_count = 500U,
        .calibration_restart_count = 1U,
        .calibration_progress_permille = 1000U,
        .calibration_ready = true,
        .attitude = {
            .filtered_gyroscope_dps = {1.0F, -2.0F, 3.0F},
            .roll_degrees = 1.25F,
            .pitch_degrees = -2.5F,
            .source_sequence = UINT64_MAX,
            .valid = true,
        },
        .processing_statistics = {
            .processed_sample_count = 9U,
            .duplicate_sample_count = 2U,
            .rejected_sample_count = 3U,
            .continuity_reset_count = 4U,
        },
    };
    char response[1536];
    size_t length;

    assert(usb_imu_response_build(&diagnostics, 42U, response,
                                  sizeof(response), &length));
    assert_valid_json_line(response, length);
    assert(strstr(response, "\"command\":\"imu\"") != NULL);
    assert(strstr(response, "\"sequence\":18446744073709551615") != NULL);
    assert(strstr(response, "\"age_us\":18446744073709551615") != NULL);
    assert(strstr(response, "\"acceleration_raw\":{\"x\":1,\"y\":-2,\"z\":16384}") != NULL);
    assert(strstr(response, "\"maximum_execution_us\":14") != NULL);
    assert(strstr(response, "\"state\":\"READY\"") != NULL);
    assert(strstr(response,
                  "\"gyroscope_corrected_raw\":{\"x\":1,\"y\":-1,\"z\":4}") != NULL);
    assert(strstr(response, "\"roll_millidegrees\":1250") != NULL);
    assert(strstr(response, "\"continuity_resets\":4") != NULL);
}

static void unavailable_snapshot_uses_nulls(void)
{
    const usb_imu_diagnostics_t diagnostics = {
        .state = {.freshness = IMU_FRESHNESS_UNAVAILABLE},
    };
    char response[768];
    size_t length;

    assert(usb_imu_response_build(&diagnostics, 1U, response,
                                  sizeof(response), &length));
    assert_valid_json_line(response, length);
    assert(strstr(response, "\"available\":false") != NULL);
    assert(strstr(response, "\"sequence\":null") != NULL);
    assert(strstr(response, "\"acceleration_raw\":null") != NULL);
}

static void invalid_arguments_and_small_buffers_fail(void)
{
    const usb_imu_diagnostics_t diagnostics = {
        .state = {.freshness = IMU_FRESHNESS_UNAVAILABLE},
    };
    char response[8];
    size_t length = 9U;

    assert(!usb_imu_response_build(NULL, 0U, response,
                                   sizeof(response), &length));
    assert(!usb_imu_response_build(&diagnostics, 0U, response,
                                   sizeof(response), &length));
    assert(length == 0U);
}

int main(void)
{
    available_snapshot_is_complete();
    unavailable_snapshot_uses_nulls();
    invalid_arguments_and_small_buffers_fail();
    return 0;
}
