# USB CDC transport

This directory owns the flight firmware's CDC device identity and bounded
transmit state. It accepts complete byte lines into two statically allocated
160-byte entries, advances one asynchronous transmission from cooperative-task
context, and exposes accepted/busy/error outcomes without STM32 types in its
public API.

Receive packets are re-armed but deliberately discarded and counted until
Milestone 0.11 adds bounded framing. Hardware pins, the OTG FS instance, FIFO
allocation, and STM USB low-level callbacks remain in Flight Computer V1 board
support. See `docs/usb-cdc-logging.md` for the complete ownership and failure
policy.
