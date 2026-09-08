#include "usb_receiver_response.h"

#include "usb_cdc_transport.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static receiver_inspection_t available_inspection(void)
{
    receiver_inspection_t inspection = {
        .sequence = 7U,
        .age_us = UINT64_C(1250),
        .freshness = RECEIVER_FRESHNESS_FRESH,
        .roll = -0.5F,
        .pitch = 0.25F,
        .yaw = 0.0F,
        .throttle = 1.0F,
        .arm_switch_high = true,
        .failsafe_state = RECEIVER_FAILSAFE_LIVE,
        .failsafe_action = RECEIVER_FAILSAFE_ACTION_LIVE,
        .link_statistics_present = true,
        .uplink_rssi_dbm = -42,
        .uplink_link_quality_percent = 99U,
        .uplink_snr_db = 8,
        .uart_received_byte_count = 135014U,
        .valid_frame_count = 5000U,
        .crc_error_count = 0U,
        .framing_error_count = 0U,
        .dma_overrun_count = 0U,
        .dma_dropped_byte_count = 0U,
        .available = true,
    };
    size_t index;

    for (index = 0U; index < RECEIVER_CHANNEL_COUNT; index++) {
        inspection.channels[index] = (uint16_t)(174U + index);
    }
    return inspection;
}

static void available_response_is_exact(void)
{
    static const char expected[] =
        "{\"type\":\"response\",\"request_id\":11,"
        "\"command\":\"receiver\",\"ok\":true,"
        "\"available\":true,\"sequence\":7,\"age_us\":1250,"
        "\"freshness\":\"FRESH\","
        "\"channels\":[174,175,176,177,178,179,180,181,182,183,184,185,"
        "186,187,188,189],"
        "\"normalized\":{\"roll\":-0.500000,\"pitch\":0.250000,"
        "\"yaw\":0.000000,\"throttle\":1.000000,\"arm\":true},"
        "\"failsafe\":{\"state\":\"LIVE\",\"action\":\"LIVE\","
        "\"stage_two_latched\":false,\"recovery_ready\":false},"
        "\"link_statistics_present\":true,\"uplink_rssi_dbm\":-42,"
        "\"uplink_link_quality_percent\":99,\"uplink_snr_db\":8,"
        "\"uart_bytes\":135014,\"valid_frames\":5000,"
        "\"crc_errors\":0,\"framing_errors\":0,\"dma_overruns\":0,"
        "\"dma_bytes_dropped\":0}\n";
    receiver_inspection_t inspection = available_inspection();
    char output[USB_CDC_TRANSMIT_CAPACITY];
    size_t length;

    assert(usb_receiver_response_build(&inspection,
                                       11U,
                                       output,
                                       sizeof(output),
                                       &length));
    assert(length == sizeof(expected) - 1U);
    assert(memcmp(output, expected, length) == 0);
}

static void unavailable_response_retains_diagnostics(void)
{
    receiver_inspection_t inspection = {
        .freshness = RECEIVER_FRESHNESS_UNAVAILABLE,
        .failsafe_state = RECEIVER_FAILSAFE_STAGE_TWO_LATCHED,
        .failsafe_action = RECEIVER_FAILSAFE_ACTION_STOP,
        .stage_two_latched = true,
        .uart_received_byte_count = 12U,
    };
    char output[USB_CDC_TRANSMIT_CAPACITY];
    size_t length;

    assert(usb_receiver_response_build(&inspection,
                                       12U,
                                       output,
                                       sizeof(output),
                                       &length));
    assert(strstr(output, "\"available\":false") != NULL);
    assert(strstr(output, "\"channels\":null") != NULL);
    assert(strstr(output, "\"normalized\":null") != NULL);
    assert(strstr(output, "\"state\":\"STAGE_TWO_LATCHED\"") != NULL);
    assert(strstr(output, "\"uart_bytes\":12") != NULL);
}

static void maximum_response_fits_transport_and_invalid_data_is_rejected(void)
{
    receiver_inspection_t inspection = available_inspection();
    char output[USB_CDC_TRANSMIT_CAPACITY];
    size_t length;
    size_t index;

    inspection.sequence = UINT32_MAX;
    inspection.age_us = UINT64_MAX;
    inspection.freshness = RECEIVER_FRESHNESS_UNAVAILABLE;
    inspection.roll = -1.0F;
    inspection.pitch = -1.0F;
    inspection.yaw = -1.0F;
    inspection.throttle = 1.0F;
    inspection.failsafe_state = RECEIVER_FAILSAFE_STAGE_TWO_RECOVERING;
    inspection.failsafe_action = RECEIVER_FAILSAFE_ACTION_HOLD_LAST;
    inspection.stage_two_latched = true;
    inspection.recovery_ready = true;
    inspection.uplink_rssi_dbm = INT16_MIN;
    inspection.uplink_link_quality_percent = UINT8_MAX;
    inspection.uplink_snr_db = INT8_MIN;
    inspection.uart_received_byte_count = UINT32_MAX;
    inspection.valid_frame_count = UINT32_MAX;
    inspection.crc_error_count = UINT32_MAX;
    inspection.framing_error_count = UINT32_MAX;
    inspection.dma_overrun_count = UINT32_MAX;
    inspection.dma_dropped_byte_count = UINT32_MAX;
    for (index = 0U; index < RECEIVER_CHANNEL_COUNT; index++) {
        inspection.channels[index] = RECEIVER_RAW_CHANNEL_MAX;
    }

    assert(usb_receiver_response_build(&inspection,
                                       UINT32_MAX,
                                       output,
                                       sizeof(output),
                                       &length));
    assert(length < sizeof(output));

    inspection.roll = 1.1F;
    assert(!usb_receiver_response_build(&inspection,
                                        1U,
                                        output,
                                        sizeof(output),
                                        &length));
    assert(length == 0U);
    assert(!usb_receiver_response_build(NULL,
                                        1U,
                                        output,
                                        sizeof(output),
                                        &length));
}

int main(void)
{
    available_response_is_exact();
    unavailable_response_retains_diagnostics();
    maximum_response_fits_transport_and_invalid_data_is_rejected();
    return 0;
}
