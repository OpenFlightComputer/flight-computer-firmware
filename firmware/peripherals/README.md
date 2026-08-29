# Peripherals

Peripheral modules own bounded semantic transports and device protocols between
the application and selected board support. Their public headers do not expose
STM32 types or routed pins.

The `usb/` implementation owns CDC descriptors, fixed receive/transmit queues,
newline framing, and the strict JSON wire protocol. Flight Computer V1 board
support supplies the proven OTG FS pins and low-level STM USB port.
