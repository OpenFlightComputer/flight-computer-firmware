# Development status

## Current phase

Phase 0 — Firmware foundation.

## Current milestone

Milestone 0.1 — repository initialization: **implemented and awaiting owner review**.

## Last completed milestone

Milestone 0.1, pending review approval and a separately approved commit.

## Current implementation status

- Created the local `flight-computer-firmware` Git repository skeleton.
- Added project, roadmap, architecture, contributor, and development documentation.
- Added the required `docs/`, `firmware/`, `tools/`, and `tests/` boundaries.
- Added a minimal native CMake/Ninja preset that validates repository wiring without claiming an STM32 firmware build.
- Inspected the current organization repository inventory, authoritative KiCad project, manufacturing-test repository, STM32F405 clock/startup setup, and USB CDC implementation.
- Recorded reusable sources and deliberate non-reuse boundaries in `docs/existing-sources.md`.

No operational firmware source or hardware behavior has been implemented.

## Known issues and limitations

- No STM32 executable target, linker setup, startup source, or flashable artifact exists yet; those belong to Milestone 0.2.
- No physical-board validation was performed during this documentation-only milestone.
- The hardware repository has owner changes in its working tree. They were inspected read-only and remain untouched.
- The manufacturing-test repository contains no STM32CubeMX `.ioc` file. Its proven MCU and USB configuration is expressed directly in C, CMake, the linker script, and pinned STM32CubeF4 sources.
- The host CMake preset currently has no executable or unit-test target, so CTest correctly reports that no tests are defined.

## Open questions

- Choose the GitHub repository visibility before creating the organization remote. Public is consistent with the portfolio goal, while the hardware repository currently remains private.
- Choose an open-source license before the first public release.
- Decide in Milestone 0.2 whether to reuse the tester's pinned STM32CubeF4 dependency as a submodule at the same version or introduce a separately pinned copy with the same required components.
- Confirm whether Milestone 0.2 should preserve Arm GNU Toolchain `15.3.rel1` as an exact reproducibility requirement or accept a documented compatible version range.
- Physical SWD boot/debug validation requires the owner, board, and ST-Link; the automated work can prepare and inspect artifacts but cannot claim on-board success without that session.

## Next step

After explicit approval, Milestone 0.2 will add only the STM32F405 cross-build and smallest bootable application foundation: pinned STM32Cube sources, Arm GCC toolchain configuration, startup assembly, linker script, clock initialization, ELF/HEX/BIN generation, and SWD/debug verification instructions. It will not introduce the application scheduler, USB commands, DShot, CRSF, or sensor behavior.
