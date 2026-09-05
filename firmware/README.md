# Firmware

This directory contains the operational embedded firmware foundation.

Milestones 1.8 and 1.9 connect the reviewed motor safety gate to a complete
four-channel DShot300 TIM8/DMA backend while retaining the Phase 0 foundation:

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
    request-ID-correlated status, health, arm, and disarm through bounded JSON
    ↓
health derives overall state and bounded details from active fault records
    ↓
    responses take priority, then one JSON log enters a two-entry USB CDC queue
    ↓
1,000 Hz highest-priority motor safety synchronization
    ↓
four-channel DShot300 adapter and TIM8 update-DMA output
```

One 1,000 Hz `usb-service` task remains the sole main-context owner of command
and logging transport progression. USB callbacks only copy bytes or publish
completion flags. Fault-registry exhaustion forces terminal `FAULT`, while an
ordinary degradation does not automatically leave `ARMED`.

`flight/actuators/motor_command` defines four normalized float throttles,
monotonic timestamp and validity metadata, exact-zero canonicalization, atomic
validation, and timeout checks. No USB, receiver, or flight controller
currently publishes a nonzero command.

`flight/actuators/motor_output` accepts complete commands through injected
backend callbacks. It requires backend initialization plus an accepted initial
force-stop, revalidates into facade-owned temporary storage, copies the backend
descriptor, and exposes accepted/busy/error results without retaining caller
storage. `app/motor_control` privately owns the only production instance and
mapping. State, health, validity, and freshness must pass before submission;
periodic synchronization expires a previously accepted command even if its
producer falls silent. The production DShot300 adapter creates physical-order
tables and connects the gate to the V1 board backend.

The V1 backend configures PC6 through PC9, TIM8, and DMA2 Stream 1/Channel 7
as one four-channel update burst. It alone converts `ESC_M1`-through-`ESC_M4`
columns into CCR order and owns the memory read by DMA. Pins remain GPIO-low at
rest, normal frames run asynchronously, DMA completion/error is published
through backend status, and force-stop aborts prior work before sending a
bounded all-zero frame. See
`docs/dshot-motor-backend.md`. This is software/build verification only;
propeller-free waveform and ESC validation remain required.

`common/` contains allocation-free hardware-independent utilities, including the bounded unsigned-64 decimal formatter used in place of target long-long `printf`. `app/` contains boot/status orchestration plus health projection, command dispatch, the portable logging core, USB adapters, fault system, system state, Task registry, and cooperative scheduler. `flight/` owns hardware-independent control data and future flight behavior. Neither contains STM32 HAL calls. `peripherals/usb/` owns CDC descriptors, bounded receive/transmit state, newline framing, and the JSON wire protocol. `hardware/boards/flightcomputer_v1/` owns board identity, routed USB pins and OTG FS setup, timebase frequency selection, VBUS mode, and initialization policy. `hardware/mcu/stm32f405/` owns F405 startup support, linker layout, HAL configuration, clock implementation, TIM5 register access, and core interrupt handlers.

The USB hardware, receive/framing pattern, and JSMN parser are adapted closely from the manufacturing tester. The build intentionally excludes its session protocol, component registry, component drivers, acceptance policy, and operator workflow.
