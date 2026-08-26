# Development status

## Current phase

Phase 0 — Firmware foundation.

## Current milestone

Milestone 0.3 — hardware layer skeleton: **complete and owner-approved**.

## Last completed milestone

Milestone 0.3 — hardware layer skeleton. Physical SWD validation remains an explicitly recorded hardware follow-up because no ST-Link was connected during implementation.

## Current implementation status

- Added the generic `board.h` contract selected by the firmware build.
- Added Flight Computer V1 board support owning board identity, manufacturing/schematic revision, expected clock, initialization policy, and translation of MCU failures into board-level results.
- Added a narrow STM32F405 MCU API owning HAL initialization, clock setup, core-frequency access, and interrupt-disabled halt behavior.
- Refactored `app/main.c` to use only the board API; application code no longer includes STM32 HAL/CMSIS or directly interprets MCU behavior.
- Retained the proven 16 MHz HSE to 168 MHz system-clock implementation and debugger-visible boot/loop behavior.
- Added hardware-layer responsibility documentation at each directory boundary.
- Revalidated the current KiCad project hashes and cross-checked all V1 signal mappings against the manufacturing-test board definition and low-level sources.
- Added `docs/flightcomputer-v1-hardware.md` with installed hardware, full MCU signal mapping, resolved interface selections, deferred choices, and retained electrical concerns.

No USB application behavior, scheduler, motor output, receiver input, sensor access, logging, or flight-control behavior has been implemented.

## Known issues and limitations

- No ST-Link was connected during Milestone 0.2, as confirmed by STM32CubeProgrammer 2.23.0. SWD programming, reset, HSE startup, and debugger-visible runtime values therefore remain physical-board checks.
- The hardware repository has owner changes in its working tree. They were inspected read-only and remain untouched.
- The minimal loop intentionally consumes the processor and has no timing semantics; the cooperative scheduler replaces it in later milestones.
- No GPIO or peripheral listed in the V1 hardware map is initialized by Milestone 0.3.
- Receiver UART, motor timer/DMA, GPS PPS capture, IMU EXTI, and ADC sampling decisions remain deliberately deferred to their owning milestones.
- The host CMake preset still has no unit-test target because the current changes are hardware boundary wiring around the already-built startup path rather than portable logic.

## Open questions

- Choose an open-source license before declaring the public source licensed for reuse.
- Decide later whether the exact GCC `15.3.1` reproducibility check should become a documented compatible version range; Milestone 0.2 deliberately matches the proven tester toolchain.
- Complete the documented SWD flash/debug checks when the board and ST-Link are connected.
- Confirm the documented LED polarity, WS2812 logic margin, and microSD card-detect polarity during physical validation.

## Next step

After explicit approval, Milestone 0.4 will implement the monotonic `uint64_t time_us(void)` abstraction using an appropriate STM32F405 hardware timebase. It will define and verify monotonicity, elapsed-time behavior, and a wrap-extension strategy without implementing the scheduler or protocol timing.

## Milestone 0.3 verification

- Clean Debug and Release configurations build with CMake 4.4.2, Ninja 1.13.2, and Arm GCC 15.3.1.
- Debug uses 5,064 bytes of Flash and reserves 2,072 bytes of RAM.
- Release uses 3,548 bytes of Flash and reserves 2,072 bytes of RAM.
- The Release ELF is a statically linked, 32-bit little-endian ARM EABI5 hard-float executable.
- The vector table is linked at `0x08000000`, the Thumb reset entry is `0x08000335`, and the stack top remains `0x20020000`.
- `main`, `board_initialize`, `board_halt`, `mcu_initialize`, `mcu_halt`, `system_clock_configure`, `firmware_boot_status`, and `firmware_main_loop_iterations` are retained as inspectable symbols.
- The ELF has no unresolved symbols, and ELF, HEX, BIN, map, and compile-command artifacts are present in both profiles.
- A source-boundary check confirms `firmware/app` contains no STM32/HAL includes, calls, registers, or CMSIS intrinsics; those appear only below the hardware boundary.
- The authoritative KiCad project, schematic, and PCB hashes still match the recorded hardware snapshot.
- Git whitespace validation passes.
- The host-development preset still configures and builds cleanly; CTest correctly reports no tests for this milestone.
