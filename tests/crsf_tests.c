#include "crsf_decoder.h"
#include "crsf_parser.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static size_t build_frame(uint8_t type,
                          const uint8_t *payload,
                          size_t payload_length,
                          uint8_t destination[CRSF_FRAME_CAPACITY])
{
    const size_t frame_length = payload_length + 4U;

    assert(frame_length <= CRSF_FRAME_CAPACITY);
    destination[0] = 0xC8U;
    destination[1] = (uint8_t)(payload_length + 2U);
    destination[2] = type;
    memcpy(&destination[3], payload, payload_length);
    destination[frame_length - 1U] =
        crsf_crc8(&destination[2], payload_length + 1U);
    return frame_length;
}

static void pack_channels(const uint16_t channels[CRSF_CHANNEL_COUNT],
                          uint8_t payload[22])
{
    size_t channel;

    memset(payload, 0, 22U);
    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; channel++) {
        const size_t bit_offset = channel * 11U;
        size_t bit;

        for (bit = 0U; bit < 11U; bit++) {
            if ((channels[channel] & (uint16_t)(1U << bit)) != 0U) {
                const size_t destination_bit = bit_offset + bit;

                payload[destination_bit / 8U] |=
                    (uint8_t)(1U << (destination_bit % 8U));
            }
        }
    }
}

static bool feed_frame(crsf_parser_t *parser,
                       const uint8_t *bytes,
                       size_t length,
                       crsf_frame_t *frame)
{
    bool ready = false;
    size_t index;

    for (index = 0U; index < length; index++) {
        ready = crsf_parser_push_byte(parser, bytes[index], frame) || ready;
    }
    return ready;
}

static void test_crc_matches_dvb_s2_check_value(void)
{
    static const uint8_t check[] = "123456789";

    assert(crsf_crc8(check, sizeof(check) - 1U) == 0xBCU);
    assert(crsf_crc8(NULL, 1U) == 0U);
}

static void test_packed_channels_round_trip(void)
{
    static const uint16_t expected[CRSF_CHANNEL_COUNT] = {
        172U, 1811U, 992U, 0U,    2047U, 300U,  400U,  500U,
        600U, 700U,  800U, 900U,  1000U, 1100U, 1200U, 1300U,
    };
    uint8_t payload[22];
    uint8_t bytes[CRSF_FRAME_CAPACITY];
    crsf_parser_t parser;
    crsf_frame_t frame;
    crsf_channels_t decoded;
    size_t length;

    pack_channels(expected, payload);
    length = build_frame(CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
                         payload,
                         sizeof(payload),
                         bytes);
    crsf_parser_initialize(&parser);

    assert(feed_frame(&parser, bytes, length, &frame));
    assert(crsf_decode_channels(&frame, &decoded));
    assert(memcmp(expected, decoded.values, sizeof(expected)) == 0);
    assert(parser.valid_frame_count == 1U);
    assert(parser.crc_error_count == 0U);
}

static void test_link_statistics_decode_signed_units(void)
{
    static const uint8_t payload[] = {
        42U, 44U, 99U, 8U, 0U, 3U, 2U, 61U, 87U, 0xFBU,
    };
    const crsf_frame_t frame = {
        .type = CRSF_FRAME_TYPE_LINK_STATISTICS,
        .payload = payload,
        .payload_length = sizeof(payload),
    };
    crsf_link_statistics_t statistics;

    assert(crsf_decode_link_statistics(&frame, &statistics));
    assert(statistics.uplink_rssi_antenna_1_dbm == -42);
    assert(statistics.uplink_rssi_antenna_2_dbm == -44);
    assert(statistics.uplink_link_quality_percent == 99U);
    assert(statistics.uplink_snr_db == 8);
    assert(statistics.downlink_rssi_dbm == -61);
    assert(statistics.downlink_link_quality_percent == 87U);
    assert(statistics.downlink_snr_db == -5);
}

static void test_bad_crc_and_length_are_counted_and_recover(void)
{
    static const uint8_t payload[] = {
        42U, 44U, 99U, 8U, 0U, 3U, 2U, 61U, 87U, 0U,
    };
    uint8_t bytes[CRSF_FRAME_CAPACITY];
    crsf_parser_t parser;
    crsf_frame_t frame;
    size_t length;

    length = build_frame(CRSF_FRAME_TYPE_LINK_STATISTICS,
                         payload,
                         sizeof(payload),
                         bytes);
    crsf_parser_initialize(&parser);
    bytes[length - 1U] ^= 0x01U;
    assert(!feed_frame(&parser, bytes, length, &frame));
    assert(parser.crc_error_count == 1U);

    assert(!crsf_parser_push_byte(&parser, 0xC8U, &frame));
    assert(!crsf_parser_push_byte(&parser, 1U, &frame));
    assert(parser.framing_error_count == 1U);

    length = build_frame(CRSF_FRAME_TYPE_LINK_STATISTICS,
                         payload,
                         sizeof(payload),
                         bytes);
    assert(feed_frame(&parser, bytes, length, &frame));
    assert(parser.valid_frame_count == 1U);
}

static void test_invalid_arguments_fail_closed(void)
{
    static const uint8_t payload[22] = {0};
    crsf_parser_t parser;
    crsf_frame_t frame = {
        .type = CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
        .payload = payload,
        .payload_length = sizeof(payload),
    };
    crsf_channels_t channels;
    crsf_link_statistics_t statistics;

    crsf_parser_initialize(&parser);
    assert(!crsf_parser_push_byte(NULL, 0U, &frame));
    assert(!crsf_parser_push_byte(&parser, 0U, NULL));
    assert(!crsf_decode_channels(NULL, &channels));
    assert(!crsf_decode_channels(&frame, NULL));
    assert(!crsf_decode_link_statistics(NULL, &statistics));
    assert(!crsf_decode_link_statistics(&frame, &statistics));
}

int main(void)
{
    test_crc_matches_dvb_s2_check_value();
    test_packed_channels_round_trip();
    test_link_statistics_decode_signed_units();
    test_bad_crc_and_length_are_counted_and_recover();
    test_invalid_arguments_fail_closed();
    return 0;
}
