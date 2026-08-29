# Firmware

This directory contains the operational embedded firmware foundation.

Milestone 0.12 adds structured health reporting over the USB JSON channel:

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
successful lifecycle transition from INITIALIZING to DISARMED
    ↓
fixed-capacity fault records and critical-state policy
    ↓
fixed-capacity registry and ready-batch scheduler
    ↓
1,000 Hz, 100 Hz, and 10 Hz diagnostic counters
    ↓
seven startup records enter the fixed logging queue
    ↓
1,000 Hz background task frames/parses USB input outside interrupt context
    ↓
status, structured health, arm, and disarm dispatch through bounded JSON
    ↓
health derives overall state and bounded details from active fault records
    ↓
responses take priority, then one JSON log enters a two-entry USB CDC queue
```

`app/` contains boot/status orchestration plus health projection, command dispatch, the portable logging core, USB adapters, fault system, system state, Task registry, and cooperative scheduler. It contains no STM32 HAL calls. `peripherals/usb/` owns CDC descriptors, bounded receive/transmit state, newline framing, and the JSON wire protocol. `hardware/boards/flightcomputer_v1/` owns board identity, routed USB pins and OTG FS setup, timebase frequency selection, and initialization policy. `hardware/mcu/stm32f405/` owns F405 startup support, linker layout, HAL configuration, clock implementation, TIM5 register access, and core interrupt handlers.

The USB hardware, receive/framing pattern, and JSMN parser are adapted closely from the manufacturing tester. The build intentionally excludes its session protocol, component registry, component drivers, acceptance policy, and operator workflow.
