# STM32F405 support

This directory owns the STM32F405 memory layout, HAL configuration, core interrupt handlers, MCU initialization, interrupt-safe halt behavior, and the current clock implementation.

The clock implementation uses the Flight Computer V1 build's 16 MHz HSE to produce 168 MHz SYSCLK and the 48 MHz PLL output required by later USB support. The board layer verifies the expected system frequency after MCU initialization.

Peripheral GPIO, timer, DMA, UART, SPI, I2C, ADC, and USB implementations belong here when their milestones require them. Their V1 routing choices belong in the board directory.
