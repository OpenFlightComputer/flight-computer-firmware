# Flight logic

This directory owns hardware-independent aircraft behavior and control data.
It must not include STM32 HAL/CMSIS headers, routed pins, timer instances, DMA
streams, or transport-specific command schemas.

Milestone 1.1 begins the layer with the normalized motor command snapshot. No
control algorithm or hardware output is implemented yet.

`receiver/` defines the portable decoded-frame source used between a future
CRSF peripheral/backend and application receiver policy. It contains no UART,
protocol parsing, scheduling, diagnostics, or motor behavior.
