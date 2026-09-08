#include "crsf_receiver_source.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FAKE_STREAM_CAPACITY 700U

typedef struct {
    uint8_t bytes[FAKE_STREAM_CAPACITY];
    size_t length;
    size_t position;
    uint32_t error;
} fake_stream_t;

static bool fake_read(void *context, uint8_t *byte)
{
    fake_stream_t *stream = context;

    if ((byte == NULL) || (stream->position >= stream->length)) {
        return false;
    }
    *byte = stream->bytes[stream->position++];
    return true;
}

static uint32_t fake_error(void *context)
{
    const fake_stream_t *stream = context;

    return stream->error;
}

static crsf_byte_stream_t byte_stream_for(fake_stream_t *fake)
{
    return (crsf_byte_stream_t){
        .read = fake_read,
        .error = fake_error,
        .context = fake,
    };
}

static size_t append_frame(fake_stream_t *stream,
                           uint8_t type,
                           const uint8_t *payload,
                           size_t payload_length)
{
    const size_t start = stream->length;
    const size_t frame_length = payload_length + 4U;

    assert(start + frame_length <= sizeof(stream->bytes));
    stream->bytes[start] = 0xC8U;
    stream->bytes[start + 1U] = (uint8_t)(payload_length + 2U);
    stream->bytes[start + 2U] = type;
    memcpy(&stream->bytes[start + 3U], payload, payload_length);
    stream->bytes[start + frame_length - 1U] =
        crsf_crc8(&stream->bytes[start + 2U], payload_length + 1U);
    stream->length += frame_length;
    return start;
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

static void test_initialization_and_empty_stream(void)
{
    fake_stream_t fake = {0};
    crsf_byte_stream_t stream = byte_stream_for(&fake);
    crsf_receiver_source_t source;
    receiver_channel_frame_t frame;
    receiver_source_t interface;

    assert(!crsf_receiver_source_initialize(NULL, &stream));
    assert(!crsf_receiver_source_initialize(&source, NULL));
    stream.read = NULL;
    assert(!crsf_receiver_source_initialize(&source, &stream));
    stream = byte_stream_for(&fake);
    stream.error = NULL;
    assert(!crsf_receiver_source_initialize(&source, &stream));

    stream = byte_stream_for(&fake);
    assert(crsf_receiver_source_initialize(&source, &stream));
    interface = crsf_receiver_source_interface(&source);
    assert(interface.read == crsf_receiver_source_read);
    assert(interface.context == &source);
    assert(interface.read(interface.context, &frame) ==
           RECEIVER_SOURCE_NO_FRAME);
}

static void test_link_frame_then_channel_frame_returns_channels(void)
{
    static const uint8_t link_payload[] = {
        42U, 44U, 99U, 8U, 0U, 3U, 2U, 61U, 87U, 0xFBU,
    };
    static const uint16_t expected[CRSF_CHANNEL_COUNT] = {
        174U, 175U, 174U, 355U, 1792U, 992U, 992U, 992U,
        992U, 992U, 992U, 992U, 992U, 992U, 992U, 992U,
    };
    fake_stream_t fake = {0};
    crsf_byte_stream_t stream = byte_stream_for(&fake);
    crsf_receiver_source_t source;
    receiver_channel_frame_t frame;
    uint8_t channel_payload[22];

    append_frame(&fake,
                 CRSF_FRAME_TYPE_LINK_STATISTICS,
                 link_payload,
                 sizeof(link_payload));
    pack_channels(expected, channel_payload);
    append_frame(&fake,
                 CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
                 channel_payload,
                 sizeof(channel_payload));

    assert(crsf_receiver_source_initialize(&source, &stream));
    assert(crsf_receiver_source_read(&source, &frame) ==
           RECEIVER_SOURCE_FRAME_AVAILABLE);
    assert(memcmp(frame.channels, expected, sizeof(expected)) == 0);
    assert(source.link_statistics_valid);
    assert(source.link_statistics.uplink_link_quality_percent == 99U);
    assert(source.link_statistics_frame_count == 1U);
    assert(source.channel_frame_count == 1U);
    assert(source.parser.valid_frame_count == 2U);
    assert(source.processed_byte_count == fake.length);
}

static void test_invalid_frame_is_reported_and_valid_frame_wins(void)
{
    static const uint16_t expected[CRSF_CHANNEL_COUNT] = {992U};
    fake_stream_t fake = {0};
    crsf_byte_stream_t stream = byte_stream_for(&fake);
    crsf_receiver_source_t source;
    receiver_channel_frame_t frame;
    uint8_t payload[22];
    size_t bad_frame_start;

    pack_channels(expected, payload);
    bad_frame_start = append_frame(&fake,
                                   CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
                                   payload,
                                   sizeof(payload));
    fake.bytes[bad_frame_start + 25U] ^= 1U;

    assert(crsf_receiver_source_initialize(&source, &stream));
    source.parser.crc_error_count = UINT32_MAX;
    assert(crsf_receiver_source_read(&source, &frame) ==
           RECEIVER_SOURCE_INVALID_FRAME);
    assert(source.parser.crc_error_count == UINT32_MAX);

    fake = (fake_stream_t){0};
    stream = byte_stream_for(&fake);
    bad_frame_start = append_frame(&fake,
                                   CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
                                   payload,
                                   sizeof(payload));
    fake.bytes[bad_frame_start + 25U] ^= 1U;
    append_frame(&fake,
                 CRSF_FRAME_TYPE_RC_CHANNELS_PACKED,
                 payload,
                 sizeof(payload));
    assert(crsf_receiver_source_initialize(&source, &stream));
    assert(crsf_receiver_source_read(&source, &frame) ==
           RECEIVER_SOURCE_FRAME_AVAILABLE);
    assert(source.parser.crc_error_count == 1U);
}

static void test_error_and_byte_budget_are_bounded(void)
{
    fake_stream_t fake = {0};
    crsf_byte_stream_t stream = byte_stream_for(&fake);
    crsf_receiver_source_t source;
    receiver_channel_frame_t frame;

    memset(fake.bytes, 0x55, sizeof(fake.bytes));
    fake.length = sizeof(fake.bytes);
    assert(crsf_receiver_source_initialize(&source, &stream));
    assert(crsf_receiver_source_read(&source, &frame) ==
           RECEIVER_SOURCE_NO_FRAME);
    assert(fake.position == CRSF_RECEIVER_SOURCE_BYTE_BUDGET);

    fake.error = 7U;
    assert(crsf_receiver_source_read(&source, &frame) ==
           RECEIVER_SOURCE_ERROR);
    assert(crsf_receiver_source_read(NULL, &frame) == RECEIVER_SOURCE_ERROR);
    assert(crsf_receiver_source_read(&source, NULL) == RECEIVER_SOURCE_ERROR);
}

int main(void)
{
    test_initialization_and_empty_stream();
    test_link_frame_then_channel_frame_returns_channels();
    test_invalid_frame_is_reported_and_valid_frame_wins();
    test_error_and_byte_budget_are_bounded();
    return 0;
}
