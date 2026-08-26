# Development status

## Current phase

Phase 0 — Firmware foundation.

## Current milestone

Milestone 0.2 — STM32 build foundation: **complete and owner-approved**.

## Last completed milestone

Milestone 0.2 — STM32 build foundation. Physical SWD validation remains an explicitly recorded hardware follow-up because no ST-Link was connected during implementation.

## Current implementation status

- Added STM32 Debug and Release CMake/Ninja presets using the complete Arm GNU Toolchain `15.3.rel1` / GCC `15.3.1`.
- Pinned STM32CubeF4 `v1.28.3` at commit `94cae6e83f00e276a11957e7833c01ac3d0bd7af`, matching the manufacturing tester.
- Reused the tester's Cortex-M4 hard-float flags, dependency checks, startup source, linker foundation, clock configuration, core exception handlers, and artifact-generation approach.
- Limited the HAL build to the core, Cortex, Flash, GPIO, power, and clock modules required by this image.
- Added a minimal boot path: HAL initialization, 16 MHz HSE/PLL configuration, 168 MHz `SystemCoreClock` verification, debugger-visible status, and an observable main-loop counter.
- Added ELF, HEX, BIN, map, and compile-command output for both firmware profiles.
- Added `docs/build-and-debug.md` with dependency, build, SWD flash, and debugger verification instructions.

No USB application behavior, scheduler, motor output, receiver input, sensor access, logging, or flight-control behavior has been implemented.

## Known issues and limitations

- No ST-Link was connected during Milestone 0.2, as confirmed by STM32CubeProgrammer 2.23.0. SWD programming, reset, HSE startup, and debugger-visible runtime values therefore remain physical-board checks.
- The hardware repository has owner changes in its working tree. They were inspected read-only and remain untouched.
- The minimal loop intentionally consumes the processor and has no timing semantics; the cooperative scheduler replaces it in later milestones.
- The source tree introduces only the MCU directory needed by the build. Milestone 0.3 will complete and document the board/MCU hardware boundaries and actual routed mappings.
- The host CMake preset still has no unit-test target because Milestone 0.2 contains hardware startup code rather than a portable logic unit.

## Open questions

- Choose an open-source license before declaring the public source licensed for reuse.
- Decide later whether the exact GCC `15.3.1` reproducibility check should become a documented compatible version range; Milestone 0.2 deliberately matches the proven tester toolchain.
- Complete the documented SWD flash/debug checks when the board and ST-Link are connected.

## Next step

After explicit approval, Milestone 0.3 will complete the `hardware/boards/flightcomputer_v1` and `hardware/mcu/stm32f405` boundaries, organize the current low-level initialization behind them, and document actual V1 mappings from the authoritative KiCad and tester sources. It will not implement DShot, CRSF, or sensor behavior.

## Milestone 0.2 verification

- Clean Debug and Release configurations build with CMake 4.4.2, Ninja 1.13.2, and Arm GCC 15.3.1.
- Debug uses 4,860 bytes of Flash and reserves 2,072 bytes of RAM.
- Release uses 3,464 bytes of Flash and reserves 2,072 bytes of RAM.
- The Release ELF is a statically linked, 32-bit little-endian ARM EABI5 hard-float executable.
- The vector table is linked at `0x08000000`, the Thumb reset entry is `0x080002e1`, and the stack top is `0x20020000`.
- `main`, `system_clock_configure`, `SysTick_Handler`, `firmware_boot_status`, and `firmware_main_loop_iterations` are retained as inspectable symbols.
- The ELF has no unresolved symbols, and ELF, HEX, BIN, map, and compile-command artifacts are present in both profiles.
- The host-development preset still configures and builds cleanly; CTest correctly reports no tests for this milestone.
