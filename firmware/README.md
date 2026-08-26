# Firmware

This directory contains the operational embedded firmware foundation.

Milestone 0.3 organizes the smallest STM32F405 image behind explicit board and MCU boundaries:

```text
application main
    ↓
Flight Computer V1 board initialization
    ↓
STM32F405 initialization
    ↓
HAL initialization and SysTick
    ↓
16 MHz HSE + PLL clock configuration
    ↓
168 MHz core-clock verification
    ↓
board clock verification
    ↓
debugger-visible running state and loop counter
```

`app/` contains only this temporary boot entry and status model. It depends on the selected board API and contains no STM32 HAL calls. `hardware/boards/flightcomputer_v1/` owns board identity and initialization policy. `hardware/mcu/stm32f405/` owns F405 startup support, linker layout, HAL configuration, clock implementation, and core interrupt handlers.

The build intentionally excludes the manufacturing tester's session protocol, component registry, USB implementation, device drivers, and operator workflow.
