# Architecture boundaries

Milestone 0.13 integrates and reviews structured health, the bounded request-ID-correlated USB JSON channel, non-blocking logging backend, structured fault policy, central application state machine, cooperative scheduler, portable Task contract, monotonic clock, and hardware boundaries.

## Dependency direction

```text
app -> flight -> peripherals -> hardware/boards -> hardware/mcu
```

The arrows indicate allowed knowledge toward increasingly hardware-specific implementation. A module may call a lower layer directly when an intermediate layer adds no useful meaning.

`common/` sits below every layer in this chain. It contains only small,
allocation-free, hardware-independent utilities and may not depend upward on
application, flight, peripheral, board, MCU, or vendor code.

| Area | Owns | Does not own |
| --- | --- | --- |
| `common/` | Bounded generic helpers such as unsigned-64 decimal conversion | Application policy, protocols, board choices, or hardware access |
| `app/` | Startup orchestration, scheduler integration, system state, fault coordination | Device protocols or routed pins |
| `flight/` | Hardware-independent control input, mixing, estimation, and control policy | STM32 HAL or PCB routing |
| `peripherals/` | Semantic transports and external-device protocols such as USB CDC, DShot, CRSF, BMI270, and BMP388 | Board pin and peripheral-instance selection |
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

Milestone 0.7 gives `app/` one event-driven lifecycle authority. Successful
startup progresses from `BOOT` through `INITIALIZING` to `DISARMED`; startup or
runtime fatal paths enter terminal `FAULT`. The module exposes explicit arm,
disarm, and failsafe events. Milestone 0.11 supplies USB as an explicit
main-context arm/disarm event source. Detailed boot diagnostics remain separate
from lifecycle state. Future
actuator implementations must gate final hardware demand on `ARMED`, as
specified in `docs/safety.md`.

Milestone 0.8 keeps fault detection separate from safety classification. A
static application catalogue maps IDs to severity and source; producers report
IDs and optional context but cannot choose their response. The fixed-capacity
fault registry owns active diagnostic records. Critical reports synchronously
send `FAULT_DETECTED` to the state authority after preserving the record.
Warning and ordinary-fault records leave lifecycle state unchanged. See
`docs/fault-system.md` for timing, overflow, clearing, and concurrency policy.

Milestone 0.9 keeps producers independent of output transport. Application and
future flight/peripheral code call the central logging facade, which captures a
bounded structured record into RAM. A backend consumes at most one immutable
record per drain attempt and must return without waiting. The current firmware
defines no transport behavior itself.
Core state, fault, and scheduler modules do not depend on the logger, avoiding
hidden diagnostic coupling in portable policy code. See `docs/logging.md` for
filtering, formatting, overflow, sequence, and concurrency rules.

Milestone 0.10 adapts the tester-proven OTG FS/CDC hardware path while replacing
its unrestricted application-loop and session-protocol ownership. A
lowest-priority 1,000 us task advances the USB transmit state and makes one
logging drain attempt. The application-facing backend formats a record and the
transport copies it before acceptance, so asynchronous USB never retains
logging-queue storage. USB initialization failure is a non-critical diagnostic
fault and does not prevent successful startup. See
`docs/usb-cdc-logging.md` for ownership, disconnect, interrupt, and physical
verification boundaries.

Milestone 0.11 keeps USB interrupt work to bounded byte copying and flag
updates. The background `usb-service` task frames at most 64 bytes per release,
parses at most one strict request, sends or retains one response, and then
attempts one log drain. Command dispatch lives in `app/` because it coordinates
state and fault authorities; newline framing and the wire protocol live in
`peripherals/usb/`. Fixed queues and discard-through-newline recovery prevent
partial or unbounded input from becoming a command. See
`docs/usb-json-protocol.md` for schemas and bounds.

Milestone 0.12 keeps health evaluation in `app/health.c`: it scans the existing
fixed fault registry and lifecycle authority without mutating either. The
application USB adapter owns bounded serialization of that result and complete
fault objects. This prevents the peripheral protocol layer from depending on
application fault types and prevents the fault system from acquiring transport
knowledge. See `docs/health-reporting.md` for precedence and completeness.

Milestone 0.13 preserves one shared `usb-service` task as the sole main-context
transport owner. Command and log work are bounded stages of that task rather
than competing scheduled tasks over shared queues. Request IDs belong to the
peripheral wire envelope and are passed to application response serializers;
they do not enter lifecycle or fault policy. Cross-module host tests exercise
startup and runtime state/fault/health chains, while physical execution-time,
stack, USB, and board evidence remains tracked in
`docs/hardware-validation-checklist.md`.

Milestone 1.1 begins `flight/` with a normalized four-motor value snapshot.
The model validates and timestamps complete commands but owns no global state,
transport, lifecycle decision, DShot representation, or hardware output. This
keeps future USB and control producers on one command type while leaving the
final motor safety gate and peripheral/hardware implementations in their
reviewed later milestones. See `docs/motor-command.md`.

Milestone 1.2 adds an instance-based facade in `flight/actuators` whose injected
backend callbacks form the downward dependency boundary. The facade copies the
backend descriptor and passes a revalidated facade-owned complete command;
accepted asynchronous backends must copy before returning. Force-stop is a
separate accepted-or-error operation so it cannot be delayed by normal busy
backpressure. A future application-owned adapter will bridge this flight type to
the selected lower peripheral API, preventing DShot from depending upward on
`flight/`. No production backend or safety authorization is connected. See
`docs/motor-output.md`.

Milestone 1.7 adds the application-owned `motor_control` safety boundary. It
privately owns the one production motor-output instance and mapping, and is the
only production module permitted to call raw motor submission. State, health,
complete-command validity, and freshness must all pass before forwarding. A
periodic synchronization call expires the last accepted command even when
producers fall silent. CTest scans production sources for accidental raw motor
or DShot calls outside their allowed owner files. See
`docs/motor-safety-gate.md`.

## Separation from manufacturing test

The manufacturing-test application proves board capabilities through an operator-driven, single-active-component workflow. Flight firmware has different lifecycle, timing, fault, safety, and data-ownership requirements. It may reuse verified clock, linker, startup, USB, and peripheral knowledge, but it will not inherit the tester's application loop, component-test registry, session protocol, or acceptance policy.
