#include "receiver_normalization.h"

#include <assert.h>
#include <stddef.h>

static void assert_close(float actual, float expected)
{
    const float difference = actual > expected ? actual - expected :
                                                 expected - actual;

    assert(difference < 0.0001f);
}

static receiver_channel_frame_t centered_frame(void)
{
    receiver_channel_frame_t frame = {0};
    size_t channel;

    for (channel = 0U; channel < RECEIVER_CHANNEL_COUNT; channel++) {
        frame.channels[channel] = 992U;
    }
    frame.channels[2] = 174U;
    frame.channels[3] = 997U;
    return frame;
}

static void test_default_configuration_is_valid_and_copied(void)
{
    receiver_normalization_config_t config;
    receiver_normalizer_t normalizer;

    receiver_normalization_default_config(&config);
    assert(receiver_normalization_config_is_valid(&config));
    assert(config.roll.channel == 0U);
    assert(config.pitch.channel == 1U);
    assert(config.throttle.channel == 2U);
    assert(config.yaw.channel == 3U);
    assert(config.arm.channel == 4U);
    assert(config.roll.minimum == 174U);
    assert(config.roll.center == 992U);
    assert(config.roll.maximum == 1805U);
    assert(config.pitch.minimum == 175U);
    assert(config.pitch.center == 992U);
    assert(config.pitch.maximum == 1811U);
    assert(config.throttle.minimum == 174U);
    assert(config.throttle.maximum == 1785U);
    assert(config.yaw.minimum == 355U);
    assert(config.yaw.center == 997U);
    assert(config.yaw.maximum == 1713U);
    assert(receiver_normalizer_initialize(&normalizer, &config) ==
           RECEIVER_NORMALIZATION_OK);
    config.roll.reversed = true;
    assert(!normalizer.config.roll.reversed);
}

static void test_configuration_rejects_invalid_ranges_and_channels(void)
{
    receiver_normalization_config_t config;
    receiver_normalizer_t normalizer;

    assert(!receiver_normalization_config_is_valid(NULL));
    assert(receiver_normalizer_initialize(NULL, NULL) ==
           RECEIVER_NORMALIZATION_INVALID_ARGUMENT);
    assert(receiver_normalizer_initialize(&normalizer, NULL) ==
           RECEIVER_NORMALIZATION_INVALID_CONFIG);

    receiver_normalization_default_config(&config);
    config.roll.minimum = config.roll.center;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.pitch.maximum = RECEIVER_RAW_CHANNEL_MAX + 1U;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.throttle.minimum = config.throttle.maximum;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.arm.channel = RECEIVER_CHANNEL_COUNT;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.arm.high_minimum = 0U;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.arm.high_minimum = RECEIVER_RAW_CHANNEL_MAX + 1U;
    assert(!receiver_normalization_config_is_valid(&config));
    receiver_normalization_default_config(&config);
    config.arm.channel = config.roll.channel;
    assert(!receiver_normalization_config_is_valid(&config));
}

static void test_default_endpoints_center_and_metadata(void)
{
    receiver_normalization_config_t config;
    receiver_normalizer_t normalizer;
    receiver_channel_frame_t frame = centered_frame();
    receiver_control_snapshot_t snapshot;

    receiver_normalization_default_config(&config);
    assert(receiver_normalizer_initialize(&normalizer, &config) ==
           RECEIVER_NORMALIZATION_OK);
    assert(receiver_normalize(&normalizer, &frame, 123U, 7U, &snapshot) ==
           RECEIVER_NORMALIZATION_OK);
    assert_close(snapshot.roll, 0.0f);
    assert_close(snapshot.pitch, 0.0f);
    assert_close(snapshot.yaw, 0.0f);
    assert_close(snapshot.throttle, 0.0f);
    assert(!snapshot.arm_switch_high);
    assert(snapshot.received_at_us == 123U);
    assert(snapshot.source_sequence == 7U);
    assert(snapshot.valid);

    frame.channels[0] = 174U;
    frame.channels[1] = 1811U;
    frame.channels[2] = 1785U;
    frame.channels[3] = 355U;
    frame.channels[4] = 1500U;
    assert(receiver_normalize(&normalizer, &frame, 456U, 8U, &snapshot) ==
           RECEIVER_NORMALIZATION_OK);
    assert_close(snapshot.roll, -1.0f);
    assert_close(snapshot.pitch, 1.0f);
    assert_close(snapshot.yaw, -1.0f);
    assert_close(snapshot.throttle, 1.0f);
    assert(snapshot.arm_switch_high);
}

static void test_clamping_reversal_and_asymmetric_centers(void)
{
    receiver_normalization_config_t config;
    receiver_normalizer_t normalizer;
    receiver_channel_frame_t frame = centered_frame();
    receiver_control_snapshot_t snapshot;

    receiver_normalization_default_config(&config);
    config.roll.reversed = true;
    config.pitch.minimum = 100U;
    config.pitch.center = 900U;
    config.pitch.maximum = 1900U;
    config.throttle.reversed = true;
    assert(receiver_normalizer_initialize(&normalizer, &config) ==
           RECEIVER_NORMALIZATION_OK);

    frame.channels[0] = 0U;
    frame.channels[1] = 1400U;
    frame.channels[2] = 172U;
    frame.channels[3] = UINT16_MAX;
    assert(receiver_normalize(&normalizer, &frame, 0U, 0U, &snapshot) ==
           RECEIVER_NORMALIZATION_OK);
    assert_close(snapshot.roll, 1.0f);
    assert_close(snapshot.pitch, 0.5f);
    assert_close(snapshot.yaw, 1.0f);
    assert_close(snapshot.throttle, 1.0f);
}

static void test_normalization_rejects_invalid_dependencies(void)
{
    receiver_normalization_config_t config;
    receiver_normalizer_t normalizer;
    receiver_channel_frame_t frame = centered_frame();
    receiver_control_snapshot_t snapshot;

    receiver_normalization_default_config(&config);
    assert(receiver_normalizer_initialize(&normalizer, &config) ==
           RECEIVER_NORMALIZATION_OK);
    assert(receiver_normalize(NULL, &frame, 0U, 0U, &snapshot) ==
           RECEIVER_NORMALIZATION_INVALID_ARGUMENT);
    assert(receiver_normalize(&normalizer, NULL, 0U, 0U, &snapshot) ==
           RECEIVER_NORMALIZATION_INVALID_ARGUMENT);
    assert(receiver_normalize(&normalizer, &frame, 0U, 0U, NULL) ==
           RECEIVER_NORMALIZATION_INVALID_ARGUMENT);
    normalizer.initialized = false;
    assert(receiver_normalize(&normalizer, &frame, 0U, 0U, &snapshot) ==
           RECEIVER_NORMALIZATION_NOT_INITIALIZED);
}

int main(void)
{
    test_default_configuration_is_valid_and_copied();
    test_configuration_rejects_invalid_ranges_and_channels();
    test_default_endpoints_center_and_metadata();
    test_clamping_reversal_and_asymmetric_centers();
    test_normalization_rejects_invalid_dependencies();
    return 0;
}
