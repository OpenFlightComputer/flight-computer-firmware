# OpenFlightComputer Flight Firmware

This repository contains the independent operational flight-control firmware for [OpenFlightComputer](https://github.com/OpenFlightComputer). The first target is the STM32F405RGT6 on Flight Computer V1.

The project is intentionally separate from the custom board design and its manufacturing-test system. The hardware repository remains authoritative for physical routing; the tester repository provides verified bring-up knowledge and reusable low-level references. Flight firmware will define its own application, scheduling, state, fault, logging, and flight-control architecture.

## Repository ecosystem

| Repository | Responsibility |
| --- | --- |
| [`flight-computer-hardware`](https://github.com/OpenFlightComputer/flight-computer-hardware) | KiCad design, PCB, manufacturing outputs, and physical board truth |
| [`flight-computer-test`](https://github.com/OpenFlightComputer/flight-computer-test) | Manufacturing firmware, computer-side acceptance tester, and proven bring-up code |
| `flight-computer-firmware` | This project: independent operational flight-control firmware |
| [`project-documentation`](https://github.com/OpenFlightComputer/project-documentation) | Cross-repository requirements, architecture, and engineering decisions |

Future simulation and drone-platform repositories remain independent projects rather than dependencies of this initial firmware foundation.

## Planned architecture

```text
app
 |
 v
flight
 |
 v
peripherals
 |
 v
hardware/boards
 |
 v
hardware/mcu
```

- `app` will own lifecycle, scheduling, system state, faults, and initialization orchestration.
- `flight` will own hardware-independent aircraft behavior and control logic.
- `peripherals` will own semantic device and external-protocol implementations.
- `hardware/boards` will own Flight Computer V1 routing and board choices.
- `hardware/mcu` will own STM32F405-specific clocks, interrupts, DMA, timers, and low-level peripheral adapters.

These are dependency boundaries, not a requirement that every operation pass through every layer.

## Current status

Phase 1, Milestone 1.2 adds a generic instance-based motor-output facade over the normalized four-motor command. Injected initialize, complete-command submit, and unconditional force-stop callbacks keep flight producers independent from the eventual DShot/timer/DMA backend. Initialization requires an accepted initial stop, accepted asynchronous submissions must be copied, and no production backend or physical output exists yet.

Milestone 1.3 is in progress following physical V1 bring-up. VBUS behavior is
now an explicit board capability: V1 assumes VBUS is present and leaves its
defective PA9 divider unused, while corrected hardware can select sensing. A
shared bounded decimal converter also removes project-owned `%llu` dependencies
from uptime, health, and logging output. Firmware now embeds separate semantic
version and dirty-aware Git build identity fields, and `./ofc` provides reusable
build, flash, inspection, and smoke automation. The traceable Release smoke and
remaining physical boundary/stress checks remain pending.

Initialize the pinned STM32CubeF4 dependency and its two required nested dependencies:

```bash
git submodule update --init firmware/third_party/STM32CubeF4
git -C firmware/third_party/STM32CubeF4 submodule update --init \
  Drivers/CMSIS/Device/ST/STM32F4xx \
  Drivers/STM32F4xx_HAL_Driver
```

Build either firmware profile:

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug

cmake --preset firmware-release
cmake --build --preset firmware-release
```

The repository also provides one host entry point for building, flashing,
inspection, and repeatable non-arming smoke tests:

```bash
./ofc firmware build --profile debug
./ofc firmware flash --profile release
./ofc device status
./ofc device monitor
./ofc smoke --profile release
./ofc smoke --no-flash
```

`./ofc smoke` builds, programs, verifies, resets, checks `DISARMED` status and
`OK` health, and writes a machine-readable JSON report. It never arms the
flight computer. The CLI is a thin wrapper over reusable Python services in
`host_tools/openflightcomputer`, allowing a later frontend to call those APIs
directly. See [docs/host-tools.md](docs/host-tools.md) for command behavior and
extension boundaries.

Each firmware build produces ELF, HEX, BIN, map, and compile-command artifacts. Native host tests exercise the timebase, task registry, scheduler, state transitions, faults, health derivation, logging core, USB JSON serialization, newline framing, protocol parsing, and command dispatch. The image configures only the internal timebase and USB CDC pins/peripheral; there is no motor control, receiver decoding, sensor processing, or operational flight behavior yet.

See [DEVELOPMENT.md](DEVELOPMENT.md) for the review handoff, [ROADMAP.md](ROADMAP.md) for milestone boundaries, [docs/motor-command.md](docs/motor-command.md) for the normalized motor snapshot, [docs/motor-output.md](docs/motor-output.md) for the backend-independent output and ownership contract, [docs/v1-bringup-carryover.md](docs/v1-bringup-carryover.md) for the physical acceptance findings and their owning milestones, [docs/phase-0-integration-review.md](docs/phase-0-integration-review.md) for the consolidated foundation contracts, [docs/hardware-validation-checklist.md](docs/hardware-validation-checklist.md) for remaining flight-image checks, [docs/health-reporting.md](docs/health-reporting.md) for health derivation and response bounds, [docs/usb-json-protocol.md](docs/usb-json-protocol.md) for commands and framing, [docs/logging.md](docs/logging.md) for logging policy and usage, [docs/usb-cdc-logging.md](docs/usb-cdc-logging.md) for USB transport behavior, [docs/fault-system.md](docs/fault-system.md) for fault policy and records, [docs/safety.md](docs/safety.md) for lifecycle safety behavior, [docs/scheduler.md](docs/scheduler.md) for scheduler behavior, [docs/task-model.md](docs/task-model.md) for the task contract, [docs/timebase.md](docs/timebase.md) for the clock design, [docs/build-and-debug.md](docs/build-and-debug.md) for setup, flashing, and debugger checks, [docs/architecture.md](docs/architecture.md) for responsibility rules, [docs/flightcomputer-v1-hardware.md](docs/flightcomputer-v1-hardware.md) for the reviewed board map, and [docs/existing-sources.md](docs/existing-sources.md) for the inspected hardware and tester evidence.
