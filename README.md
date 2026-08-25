# OpenFlightComputer Flight Firmware

This repository will contain the independent operational flight-control firmware for [OpenFlightComputer](https://github.com/OpenFlightComputer). The first target is the STM32F405RGT6 on Flight Computer V1.

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

Phase 0, Milestone 0.1 establishes documentation, repository boundaries, and CMake scaffolding only. There is no STM32 target, bootable image, scheduler, motor control, receiver decoding, or sensor processing yet.

The current host preset verifies that the repository skeleton configures cleanly:

```bash
cmake --preset host-development
cmake --build --preset host-development
ctest --preset host-development
```

Milestone 0.2 will add the STM32CubeF4/STM32F405 cross-build, linker/startup foundation, and smallest boot path after owner approval.

See [DEVELOPMENT.md](DEVELOPMENT.md) for the review handoff, [ROADMAP.md](ROADMAP.md) for milestone boundaries, [docs/architecture.md](docs/architecture.md) for initial responsibility rules, and [docs/existing-sources.md](docs/existing-sources.md) for the inspected hardware and tester evidence.
