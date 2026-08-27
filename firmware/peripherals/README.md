# Peripherals

Peripheral modules own bounded semantic transports and device protocols between
the application and selected board support. Their public headers do not expose
STM32 types or routed pins.

Milestone 0.10 adds the first implementation: `usb/` owns CDC descriptors and
the fixed transmit queue. Flight Computer V1 board support supplies the proven
OTG FS pins and low-level STM USB port.
