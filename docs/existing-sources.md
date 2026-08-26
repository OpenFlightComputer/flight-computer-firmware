# Existing source inspection

This record identifies the sources inspected for Phase 0, Milestone 0.1. Absolute paths describe the current development machine; repository links provide durable project identities.

## GitHub organization

The authenticated organization inventory inspected on 2026-08-25 contained:

| Repository | Visibility | Default branch | Role |
| --- | --- | --- | --- |
| [`flight-computer-test`](https://github.com/OpenFlightComputer/flight-computer-test) | Public | `main` | Manufacturing and acceptance test |
| [`project-documentation`](https://github.com/OpenFlightComputer/project-documentation) | Public | `main` | System-level documentation |
| [`flight-computer-hardware`](https://github.com/OpenFlightComputer/flight-computer-hardware) | Private | `main` | Hardware design |

No `flight-computer-firmware` organization repository existed at inspection time. Milestone 0.1 subsequently created it as a public repository at <https://github.com/OpenFlightComputer/flight-computer-firmware>.

## Authoritative Flight Computer V1 hardware

- Local root: `/Users/jul/Private/projects/flightComputer/flight_computer_pcb/FlightComputer_V1`
- Remote: `git@github.com:OpenFlightComputer/flight-computer-hardware.git`
- Branch and commit: `main`, `e1f4bdebb0fe18a274fba08a50c99ed978c7ec72`
- Working state: modified and untracked owner files were present; this milestone made no hardware changes
- Latest root KiCad project: `FlightComputer_V1.kicad_pro` with `FlightComputer_V1.kicad_sch`, hierarchical sheets, and `FlightComputer_V1.kicad_pcb`
- Root file modification time: 2026-08-10 17:39:51 +0200 for the project and PCB, one second earlier for the root schematic

Exact current hashes:

| File | SHA-256 |
| --- | --- |
| `FlightComputer_V1.kicad_pro` | `ed197e094f1406852b88b31015f0e1152ceef05802e561fce1d1354021c72d44` |
| `FlightComputer_V1.kicad_sch` | `90c675907b837f97d827d3e5cbc24ff5cf763a53324cc96fd155d12ba1f80347` |
| `FlightComputer_V1.kicad_pcb` | `3f0a6b184265de4666bb49451353d1cc9ca22d569f1a510fe64b1ad4d7d17f23` |

The detailed MCU net map and hardware observations already captured in `flight-computer-test/docs/hardware-reference.md` were checked against this unchanged hash set. The first firmware target remains STM32F405RGT6 on manufacturing revision 1.7, generated from a schematic whose title block remains revision 0.1.

## Manufacturing-test reference

- Local root: `/Users/jul/Private/projects/OpenFlightComputer/flight-computer-test`
- Remote: `git@github.com:OpenFlightComputer/flight-computer-test.git`
- Inspected commit: `3bdc93d0d00cc9052985b2ffa765f4911f8a5c93`
- Working state during inspection: clean
- Firmware root: `firmware/manufacturing_test`

### Build and MCU foundation worth reusing

| Concern | Proven source | Reuse intent |
| --- | --- | --- |
| CMake source graph | `firmware/manufacturing_test/CMakeLists.txt` | Adapted build conventions, strict warnings, artifacts, and pinned dependency checks |
| Arm GCC discovery and Cortex-M4 flags | `firmware/manufacturing_test/cmake/arm-none-eabi-gcc.cmake` | Reused unchanged in Milestone 0.2 |
| Debug/Release presets | `firmware/manufacturing_test/CMakePresets.json` | Adapted for repository-root firmware and host workflows |
| Flash/RAM layout | `firmware/manufacturing_test/board_support/flightcomputer_v1/STM32F405RGTX_FLASH.ld` | Reused with a zero-byte requested heap and without tester metadata |
| Startup source | Pinned STM32CubeF4 `startup_stm32f405xx.s` selected by CMake | Reused from the identical vendor pin |
| HAL configuration | `firmware/manufacturing_test/board_support/flightcomputer_v1/stm32f4xx_hal_conf.h` | Reduced to modules required by the minimal boot image |
| Interrupt foundation | `firmware/manufacturing_test/board_support/flightcomputer_v1/stm32f4xx_it.c` | Reused core handlers; component-specific DMA and USB handlers excluded |
| System clock | `firmware/manufacturing_test/board_support/flightcomputer_v1/system_clock.c` | Reused unchanged |
| Minimal boot sequence | `firmware/manufacturing_test/application/main.c` | Reused initialization order with a flight-specific debug status only |

The proven clock tree is 16 MHz HSE divided by 16, multiplied by 336, and divided by 2 for 168 MHz SYSCLK. AHB is 168 MHz, APB1 is 42 MHz, APB2 is 84 MHz, and PLLQ 7 supplies the required 48 MHz USB clock. Clock failure is deliberately observable rather than hidden behind an HSI fallback.

The tester pins STM32CubeF4 `v1.28.3` at commit `94cae6e83f00e276a11957e7833c01ac3d0bd7af`, with its required CMSIS/STM32F4 HAL dependencies. It builds successfully with Arm GNU Toolchain `15.3.rel1` / GCC `15.3.1`. There is no `.ioc` file in the repository: this proven configuration is directly maintained in C, CMake, the linker script, and the pinned vendor sources.

### Working USB implementation worth reusing later

| Concern | Proven source | Reuse intent |
| --- | --- | --- |
| OTG FS pins, VBUS sense, PCD and FIFO setup | `firmware/manufacturing_test/board_support/flightcomputer_v1/usb_device_port.c` | Adapt board/MCU setup for the Phase 0 USB backend |
| OTG FS IRQ forwarding | `firmware/manufacturing_test/board_support/flightcomputer_v1/stm32f4xx_it.c` | Adapt the short interrupt entry point |
| Static CDC transport queues | `firmware/manufacturing_test/protocol/usb_cdc_transport.c` | Reuse bounded, non-blocking design patterns rather than tester protocol semantics |
| Newline framing | `firmware/manufacturing_test/protocol/newline_framer.c` | Candidate reusable hardware-independent implementation for the later JSON-command milestone |
| USB descriptors | `firmware/manufacturing_test/protocol/usb_descriptors.c` | Adapt with a distinct flight-firmware product identity |

The working path uses PA11/PA12 as OTG FS DM/DP on AF10, PA9 for VBUS sensing, Full-Speed CDC ACM, static ST USB class storage, a 512-byte ISR-to-main receive ring, bounded complete-line and transmit queues, and parsing/transmit scheduling outside the interrupt. This design is relevant to later flight USB and logging work because slow computer I/O never blocks inside the producer call path.

### Knowledge to retain without implementing now

The tester also proves current board mappings and low-level access for BMI270 over SPI3, BMP388 over I2C2, microSD over SPI1, status LEDs, and WS2812 timer/DMA output. Those sources remain references for their future approved milestones. Milestone 0.1 does not copy or execute any sensor, storage, LED, DShot, receiver, or flight-control code.

### Tester architecture deliberately not reused

Flight firmware will not copy the tester's component registry, single-active-test runner, manufacturing session metadata, acceptance-test JSON messages, operator workflow, or host-side test policy. These are correct for manufacturing acceptance but do not model deterministic flight tasks, system state, faults, control ownership, or actuator safety.
