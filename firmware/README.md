# Firmware

This directory contains the operational embedded firmware foundation.

Milestone 0.2 builds the smallest STM32F405 image:

```text
STM32 startup
    ↓
HAL initialization and SysTick
    ↓
16 MHz HSE + PLL clock configuration
    ↓
168 MHz core-clock verification
    ↓
debugger-visible running state and loop counter
```

`app/` contains only this temporary boot entry and status model. `hardware/mcu/stm32f405/` contains the F405 startup support, linker layout, HAL configuration, clock implementation, and core interrupt handlers. Milestone 0.3 will complete the board-versus-MCU separation before peripheral work begins.

The build intentionally excludes the manufacturing tester's session protocol, component registry, USB implementation, device drivers, and operator workflow.
