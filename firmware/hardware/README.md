# Hardware

Hardware code isolates the operational application from PCB routing and STM32 implementation details.

```text
application
    ↓
selected board
    ↓
selected MCU
```

`boards/` owns physical product identity, installed hardware, routed signals, and board-level initialization policy. `mcu/` owns processor-family startup, clocks, interrupts, memory layout, and low-level capability implementations.

Only Flight Computer V1 and STM32F405 exist today. Additional directories should be added only for real supported hardware.
