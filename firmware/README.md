# Firmware

This directory contains the operational embedded firmware foundation.

Milestone 0.4 adds the monotonic clock to the STM32F405 image behind the existing board and MCU boundaries:

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
TIM5 configured as a 1 MHz free-running counter
    ↓
race-safe 32-to-64-bit microsecond extension
    ↓
debugger-visible running state, loop counter, and uptime
```

`app/` contains only this temporary boot entry and status model. It depends on generic board and time APIs and contains no STM32 HAL calls. `hardware/boards/flightcomputer_v1/` owns board identity, timebase frequency selection, and initialization policy. `hardware/mcu/stm32f405/` owns F405 startup support, linker layout, HAL configuration, clock implementation, TIM5 register access, and core interrupt handlers.

The build intentionally excludes the manufacturing tester's session protocol, component registry, USB implementation, device drivers, and operator workflow.
