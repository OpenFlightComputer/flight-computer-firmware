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

Phase 0, Milestone 0.10 connects the fixed logging queue to a non-blocking USB CDC backend adapted from the proven manufacturing tester. A lowest-priority 1,000 Hz task moves at most one canonical log line per release into a two-entry USB-owned transmit queue. Disconnects and congestion never wait in the logging path, and USB initialization failure records a non-critical fault while firmware continues.

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

Each firmware build produces ELF, HEX, BIN, map, and compile-command artifacts. Native host tests exercise the timebase, task registry, scheduler, state transitions, faults, logging core, and USB logging adapter. The image configures only the internal timebase and USB CDC pins/peripheral; there is no motor control, receiver decoding, sensor processing, USB command protocol, or operational flight behavior yet.

See [DEVELOPMENT.md](DEVELOPMENT.md) for the review handoff, [ROADMAP.md](ROADMAP.md) for milestone boundaries, [docs/logging.md](docs/logging.md) for logging policy and usage, [docs/usb-cdc-logging.md](docs/usb-cdc-logging.md) for USB transport behavior, [docs/fault-system.md](docs/fault-system.md) for fault policy and records, [docs/safety.md](docs/safety.md) for lifecycle safety behavior, [docs/scheduler.md](docs/scheduler.md) for scheduler behavior, [docs/task-model.md](docs/task-model.md) for the task contract, [docs/timebase.md](docs/timebase.md) for the clock design, [docs/build-and-debug.md](docs/build-and-debug.md) for setup, flashing, and debugger checks, [docs/architecture.md](docs/architecture.md) for responsibility rules, [docs/flightcomputer-v1-hardware.md](docs/flightcomputer-v1-hardware.md) for the reviewed board map, and [docs/existing-sources.md](docs/existing-sources.md) for the inspected hardware and tester evidence.
