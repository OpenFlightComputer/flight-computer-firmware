# Architecture boundaries

Milestone 0.6 adds the cooperative scheduler over the portable Task contract, central monotonic clock, and hardware boundaries.

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

## Current boot path

```text
app/main.c
    ↓ generic board API
hardware/boards/flightcomputer_v1/board.c
    ↓ generic MCU API
hardware/mcu/stm32f405/mcu.c
    ↓ STM32 HAL/CMSIS
STM32F405 hardware
```

The application sees `board_initialize()`, `board_halt()`, and the generic `time_us()` API. It does not include STM32 headers or interpret HAL return values.

The V1 board implementation owns board identity, expected clock frequency, the TIM5 timebase selection, and the policy that startup succeeds only when MCU initialization completes, the core reaches 168 MHz, and the timebase starts successfully. It deliberately does not configure unused peripheral pins.

The STM32F405 implementation owns HAL initialization, the PLL and bus-clock procedure, `SystemCoreClock` access, TIM5 configuration and overflow handling, interrupt shutdown, the core exception table, and the linker/startup foundation. The current clock procedure is intentionally direct rather than a speculative multi-board clock framework.

Exact V1 peripheral routing is recorded in `docs/flightcomputer-v1-hardware.md`. Routing knowledge will enter executable board support only when an approved peripheral milestone consumes it.

## Execution and ownership rules

- The initial execution backend will be a cooperative scheduler assisted by short interrupts and DMA.
- Application and flight modules will use a central monotonic microsecond API rather than MCU timer registers directly.
- Producer modules own their latest state and expose explicit typed snapshots containing validity and timestamps.
- System state has central authority, and disarm will override actuator demand once motor output exists.
- Logging producers will be bounded and non-blocking; backend latency must not delay realtime work.
- Runtime memory will use static, stack, and fixed-capacity storage rather than dynamic allocation.

The scheduler, state, fault, logging, and USB interfaces will be specified in their own Phase 0 milestones instead of being guessed early.

Milestone 0.6 instantiates Task metadata and registration within `app/` and
runs the cooperative backend from the main loop. The scheduler consumes the
generic `time_us()` API through an injected clock function and contains no
hardware knowledge. Ready batches bound selection fairness while execution
measurements expose overload rather than concealing it.

## Separation from manufacturing test

The manufacturing-test application proves board capabilities through an operator-driven, single-active-component workflow. Flight firmware has different lifecycle, timing, fault, safety, and data-ownership requirements. It may reuse verified clock, linker, startup, USB, and peripheral knowledge, but it will not inherit the tester's application loop, component-test registry, session protocol, or acceptance policy.
