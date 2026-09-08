#include "crsf_decoder.h"

#include <stddef.h>

#define CRSF_PACKED_CHANNEL_PAYLOAD_LENGTH 22U
#define CRSF_LINK_STATISTICS_PAYLOAD_LENGTH 10U
#define CRSF_CHANNEL_BITS 11U
#define CRSF_CHANNEL_MASK 0x07FFU

bool crsf_decode_channels(const crsf_frame_t *frame,
                          crsf_channels_t *channels)
{
    size_t channel;

    if ((frame == NULL) || (channels == NULL) ||
        (frame->type != CRSF_FRAME_TYPE_RC_CHANNELS_PACKED) ||
        (frame->payload == NULL) ||
        (frame->payload_length < CRSF_PACKED_CHANNEL_PAYLOAD_LENGTH)) {
        return false;
    }

    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; channel++) {
        const size_t bit_offset = channel * CRSF_CHANNEL_BITS;
        const size_t byte_offset = bit_offset / 8U;
        const uint8_t shift = (uint8_t)(bit_offset % 8U);
        uint32_t packed = frame->payload[byte_offset];

        if (byte_offset + 1U < CRSF_PACKED_CHANNEL_PAYLOAD_LENGTH) {
            packed |= (uint32_t)frame->payload[byte_offset + 1U] << 8U;
        }
        if (byte_offset + 2U < CRSF_PACKED_CHANNEL_PAYLOAD_LENGTH) {
            packed |= (uint32_t)frame->payload[byte_offset + 2U] << 16U;
        }
        channels->values[channel] =
            (uint16_t)((packed >> shift) & CRSF_CHANNEL_MASK);
    }
    return true;
}

bool crsf_decode_link_statistics(const crsf_frame_t *frame,
                                 crsf_link_statistics_t *statistics)
{
    if ((frame == NULL) || (statistics == NULL) ||
        (frame->type != CRSF_FRAME_TYPE_LINK_STATISTICS) ||
        (frame->payload == NULL) ||
        (frame->payload_length < CRSF_LINK_STATISTICS_PAYLOAD_LENGTH)) {
        return false;
    }

    statistics->uplink_rssi_antenna_1_dbm = -(int16_t)frame->payload[0];
    statistics->uplink_rssi_antenna_2_dbm = -(int16_t)frame->payload[1];
    statistics->uplink_link_quality_percent = frame->payload[2];
    statistics->uplink_snr_db = (int8_t)frame->payload[3];
    statistics->active_antenna = frame->payload[4];
    statistics->rf_profile = frame->payload[5];
    statistics->uplink_transmit_power = frame->payload[6];
    statistics->downlink_rssi_dbm = -(int16_t)frame->payload[7];
    statistics->downlink_link_quality_percent = frame->payload[8];
    statistics->downlink_snr_db = (int8_t)frame->payload[9];
    return true;
}
