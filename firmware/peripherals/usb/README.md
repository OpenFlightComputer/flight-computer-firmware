# USB CDC transport

This directory owns the flight firmware's CDC device identity, bounded
receive/transmit state, newline framing, and strict JSON wire protocol. It
accepts complete output lines into two statically allocated 768-byte entries,
advances asynchronous transfers from cooperative-task context, and exposes
accepted/busy/error outcomes without STM32 types in its public API.

The receive callback copies bytes into a 512-byte ring and re-arms immediately.
Main context frames lines up to 256 bytes into a two-entry queue and counts raw,
line-queue, and oversized-line drops. Hardware pins, OTG FS, FIFO allocation,
and STM USB callbacks remain in Flight Computer V1 board support. See
`docs/usb-json-protocol.md` and `docs/usb-cdc-logging.md` for protocol,
ownership, and failure policy.
