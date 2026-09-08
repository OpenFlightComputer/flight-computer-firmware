# CRSF receiver transport

This directory contains the hardware-independent part of the receiver path.
The parser incrementally recognizes CRSF frames and validates CRC-8/DVB-S2.
The decoder extracts packed 16-channel and link-statistics frames. The source
adapter drains a generic byte stream and exposes at most one decoded channel
frame through `receiver_source_t` per call.

The adapter examines at most 512 bytes in one call. A valid channel frame wins
over malformed data encountered earlier in the same call; otherwise malformed
data is reported without replacing the application's last valid snapshot.
Parser, channel, link-statistics, byte, and error counters are retained for
diagnostics and saturate instead of wrapping where applicable.

The implementation was adapted from the physically exercised manufacturing
tester. STM32 GPIO, UART, DMA, and interrupt ownership remain in the selected
board backend rather than in this protocol module.
