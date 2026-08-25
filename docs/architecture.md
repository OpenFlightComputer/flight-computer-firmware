# Initial architecture boundaries

Milestone 0.1 establishes responsibility and dependency rules; it does not yet introduce application modules.

## Dependency direction

```text
app -> flight -> peripherals -> hardware/boards -> hardware/mcu
```

The arrows indicate allowed knowledge toward increasingly hardware-specific implementation. A module may call a lower layer directly when an intermediate layer adds no useful meaning.

| Area | Owns | Does not own |
| --- | --- | --- |
| `app/` | Startup orchestration, scheduler integration, system state, fault coordination | Device protocols or routed pins |
| `flight/` | Hardware-independent control input, mixing, estimation, and control policy | STM32 HAL or PCB routing |
| `peripherals/` | External-device semantics such as DShot, CRSF, BMI270, and BMP388 protocols | Board pin and peripheral-instance selection |
| `hardware/boards/flightcomputer_v1/` | V1 routed pins, installed devices, peripheral selections, and board initialization policy | Generic flight behavior |
| `hardware/mcu/stm32f405/` | STM32F405 clocks, interrupt support, timers, DMA, and low-level adapters | Flight Computer V1 routing unless unavoidable |

## Execution and ownership rules

- The initial execution backend will be a cooperative scheduler assisted by short interrupts and DMA.
- Application and flight modules will use a central monotonic microsecond API rather than MCU timer registers directly.
- Producer modules own their latest state and expose explicit typed snapshots containing validity and timestamps.
- System state has central authority, and disarm will override actuator demand once motor output exists.
- Logging producers will be bounded and non-blocking; backend latency must not delay realtime work.
- Runtime memory will use static, stack, and fixed-capacity storage rather than dynamic allocation.

The task, scheduler, state, fault, logging, and USB interfaces will be specified in their own Phase 0 milestones instead of being guessed during repository initialization.

## Separation from manufacturing test

The manufacturing-test application proves board capabilities through an operator-driven, single-active-component workflow. Flight firmware has different lifecycle, timing, fault, safety, and data-ownership requirements. It may reuse verified clock, linker, startup, USB, and peripheral knowledge, but it will not inherit the tester's application loop, component-test registry, session protocol, or acceptance policy.
