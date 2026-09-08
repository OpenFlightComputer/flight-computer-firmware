#ifndef OPENFLIGHTCOMPUTER_CRSF_DECODER_H
#define OPENFLIGHTCOMPUTER_CRSF_DECODER_H

#include "crsf_parser.h"

#include <stdbool.h>
#include <stdint.h>

#define CRSF_CHANNEL_COUNT 16U
#define CRSF_FRAME_TYPE_LINK_STATISTICS 0x14U
#define CRSF_FRAME_TYPE_RC_CHANNELS_PACKED 0x16U

typedef struct {
    uint16_t values[CRSF_CHANNEL_COUNT];
} crsf_channels_t;

typedef struct {
    int16_t uplink_rssi_antenna_1_dbm;
    int16_t uplink_rssi_antenna_2_dbm;
    uint8_t uplink_link_quality_percent;
    int8_t uplink_snr_db;
    uint8_t active_antenna;
    uint8_t rf_profile;
    uint8_t uplink_transmit_power;
    int16_t downlink_rssi_dbm;
    uint8_t downlink_link_quality_percent;
    int8_t downlink_snr_db;
} crsf_link_statistics_t;

bool crsf_decode_channels(const crsf_frame_t *frame,
                          crsf_channels_t *channels);
bool crsf_decode_link_statistics(const crsf_frame_t *frame,
                                 crsf_link_statistics_t *statistics);

#endif
