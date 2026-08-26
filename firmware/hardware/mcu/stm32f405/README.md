# STM32F405 support

This directory owns the STM32F405 memory layout, HAL configuration, core interrupt handlers, MCU initialization, interrupt-safe halt behavior, and the current clock implementation.

The clock implementation uses the Flight Computer V1 build's 16 MHz HSE to produce 168 MHz SYSCLK and the 48 MHz PLL output required by later USB support. The board layer verifies the expected system frequency after MCU initialization.

Milestone 0.4 adds the TIM5 backend for the monotonic microsecond clock. It validates the active APB1-derived timer clock, configures TIM5 as a 1 MHz free-running 32-bit counter, handles its overflow interrupt, and uses a host-tested snapshot resolver for race-safe 64-bit reads.

Future peripheral GPIO, timer, DMA, UART, SPI, I2C, ADC, and USB implementations belong here when their milestones require them. Their V1 routing choices belong in the board directory.
