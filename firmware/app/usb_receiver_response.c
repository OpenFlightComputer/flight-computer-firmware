#include "usb_receiver_response.h"

#include "uint64_decimal.h"

#include <stdio.h>

#define NORMALIZED_SCALE 1000000L
#define NORMALIZED_TEXT_CAPACITY 12U

static bool normalized_format(float value,
                              float minimum,
                              float maximum,
                              char *destination,
                              size_t capacity)
{
    long scaled;
    unsigned long magnitude;
    const char *sign;
    int written;

    if ((destination == NULL) || (capacity == 0U) ||
        !(value >= minimum) || !(value <= maximum)) {
        return false;
    }

    scaled = (long)(value * (float)NORMALIZED_SCALE +
                    (value < 0.0F ? -0.5F : 0.5F));
    sign = scaled < 0L ? "-" : "";
    magnitude = (unsigned long)(scaled < 0L ? -scaled : scaled);
    written = snprintf(destination,
                       capacity,
                       "%s%lu.%06lu",
                       sign,
                       magnitude / (unsigned long)NORMALIZED_SCALE,
                       magnitude % (unsigned long)NORMALIZED_SCALE);
    return (written >= 0) && ((size_t)written < capacity);
}

static bool finish_response(int written, size_t capacity, size_t *length)
{
    if ((written < 0) || ((size_t)written >= capacity)) {
        if (length != NULL) {
            *length = 0U;
        }
        return false;
    }

    *length = (size_t)written;
    return true;
}

bool usb_receiver_response_build(const receiver_inspection_t *inspection,
                                 uint32_t request_id,
                                 char *destination,
                                 size_t capacity,
                                 size_t *length)
{
    char age[UINT64_DECIMAL_BUFFER_CAPACITY];
    char roll[NORMALIZED_TEXT_CAPACITY];
    char pitch[NORMALIZED_TEXT_CAPACITY];
    char yaw[NORMALIZED_TEXT_CAPACITY];
    char throttle[NORMALIZED_TEXT_CAPACITY];
    size_t age_length;
    int written;

    if ((inspection == NULL) || (destination == NULL) ||
        (capacity == 0U) || (length == NULL) ||
        ((unsigned int)inspection->freshness >= RECEIVER_FRESHNESS_COUNT) ||
        ((unsigned int)inspection->failsafe_state >=
         RECEIVER_FAILSAFE_STATE_COUNT) ||
        ((unsigned int)inspection->failsafe_action >=
         RECEIVER_FAILSAFE_ACTION_COUNT)) {
        return false;
    }

    if (!inspection->available) {
        written = snprintf(
            destination,
            capacity,
            "{\"type\":\"response\",\"request_id\":%lu,"
            "\"command\":\"receiver\",\"ok\":true,"
            "\"available\":false,\"sequence\":null,\"age_us\":null,"
            "\"freshness\":\"%s\",\"channels\":null,"
            "\"normalized\":null,\"failsafe\":{\"state\":\"%s\","
            "\"action\":\"%s\",\"stage_two_latched\":%s,"
            "\"recovery_ready\":%s},\"link_statistics_present\":%s,"
            "\"uplink_rssi_dbm\":%d,"
            "\"uplink_link_quality_percent\":%u,\"uplink_snr_db\":%d,"
            "\"uart_bytes\":%lu,\"valid_frames\":%lu,"
            "\"crc_errors\":%lu,\"framing_errors\":%lu,"
            "\"dma_overruns\":%lu,\"dma_bytes_dropped\":%lu}\n",
            (unsigned long)request_id,
            receiver_freshness_state_name(inspection->freshness),
            receiver_failsafe_state_name(inspection->failsafe_state),
            receiver_failsafe_action_name(inspection->failsafe_action),
            inspection->stage_two_latched ? "true" : "false",
            inspection->recovery_ready ? "true" : "false",
            inspection->link_statistics_present ? "true" : "false",
            (int)inspection->uplink_rssi_dbm,
            (unsigned int)inspection->uplink_link_quality_percent,
            (int)inspection->uplink_snr_db,
            (unsigned long)inspection->uart_received_byte_count,
            (unsigned long)inspection->valid_frame_count,
            (unsigned long)inspection->crc_error_count,
            (unsigned long)inspection->framing_error_count,
            (unsigned long)inspection->dma_overrun_count,
            (unsigned long)inspection->dma_dropped_byte_count);
        return finish_response(written, capacity, length);
    }

    if (!uint64_decimal_format(inspection->age_us,
                               0U,
                               age,
                               sizeof(age),
                               &age_length) ||
        !normalized_format(inspection->roll,
                           -1.0F,
                           1.0F,
                           roll,
                           sizeof(roll)) ||
        !normalized_format(inspection->pitch,
                           -1.0F,
                           1.0F,
                           pitch,
                           sizeof(pitch)) ||
        !normalized_format(inspection->yaw,
                           -1.0F,
                           1.0F,
                           yaw,
                           sizeof(yaw)) ||
        !normalized_format(inspection->throttle,
                           0.0F,
                           1.0F,
                           throttle,
                           sizeof(throttle))) {
        *length = 0U;
        return false;
    }

    written = snprintf(
        destination,
        capacity,
        "{\"type\":\"response\",\"request_id\":%lu,"
        "\"command\":\"receiver\",\"ok\":true,"
        "\"available\":true,\"sequence\":%lu,\"age_us\":%s,"
        "\"freshness\":\"%s\",\"channels\":["
        "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],"
        "\"normalized\":{\"roll\":%s,\"pitch\":%s,\"yaw\":%s,"
        "\"throttle\":%s,\"arm\":%s},"
        "\"failsafe\":{\"state\":\"%s\",\"action\":\"%s\","
        "\"stage_two_latched\":%s,\"recovery_ready\":%s},"
        "\"link_statistics_present\":%s,\"uplink_rssi_dbm\":%d,"
        "\"uplink_link_quality_percent\":%u,\"uplink_snr_db\":%d,"
        "\"uart_bytes\":%lu,\"valid_frames\":%lu,"
        "\"crc_errors\":%lu,\"framing_errors\":%lu,"
        "\"dma_overruns\":%lu,\"dma_bytes_dropped\":%lu}\n",
        (unsigned long)request_id,
        (unsigned long)inspection->sequence,
        age,
        receiver_freshness_state_name(inspection->freshness),
        (unsigned int)inspection->channels[0],
        (unsigned int)inspection->channels[1],
        (unsigned int)inspection->channels[2],
        (unsigned int)inspection->channels[3],
        (unsigned int)inspection->channels[4],
        (unsigned int)inspection->channels[5],
        (unsigned int)inspection->channels[6],
        (unsigned int)inspection->channels[7],
        (unsigned int)inspection->channels[8],
        (unsigned int)inspection->channels[9],
        (unsigned int)inspection->channels[10],
        (unsigned int)inspection->channels[11],
        (unsigned int)inspection->channels[12],
        (unsigned int)inspection->channels[13],
        (unsigned int)inspection->channels[14],
        (unsigned int)inspection->channels[15],
        roll,
        pitch,
        yaw,
        throttle,
        inspection->arm_switch_high ? "true" : "false",
        receiver_failsafe_state_name(inspection->failsafe_state),
        receiver_failsafe_action_name(inspection->failsafe_action),
        inspection->stage_two_latched ? "true" : "false",
        inspection->recovery_ready ? "true" : "false",
        inspection->link_statistics_present ? "true" : "false",
        (int)inspection->uplink_rssi_dbm,
        (unsigned int)inspection->uplink_link_quality_percent,
        (int)inspection->uplink_snr_db,
        (unsigned long)inspection->uart_received_byte_count,
        (unsigned long)inspection->valid_frame_count,
        (unsigned long)inspection->crc_error_count,
        (unsigned long)inspection->framing_error_count,
        (unsigned long)inspection->dma_overrun_count,
        (unsigned long)inspection->dma_dropped_byte_count);
    return finish_response(written, capacity, length);
}
