# Development status

## Current phase

Phase 0 — Firmware foundation.

## Current milestone

Milestone 0.4 — monotonic microsecond timebase: **implemented and awaiting owner review**.

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
- Added the hardware-neutral `uint64_t time_us(void)` API and integrated it into the temporary main loop as the debugger-visible `firmware_uptime_us` value.
- Reserved internal TIM5 for the V1 timebase, validated its 84 MHz APB1-derived input clock, and configured a 1 MHz free-running 32-bit counter with no GPIO or DMA use.
- Extended TIM5 to 64 bits with a short update interrupt, a software overflow word, update-pending detection, and a retrying snapshot resolver that handles an interrupt racing a read.
- Added native tests for ordinary reads, pending hardware overflow, interrupt/read races, and monotonic progression across the 32-bit boundary.
- Added `docs/timebase.md` with the clock calculation, initialization contract, overflow strategy, interrupt-priority rationale, guarantees, and physical verification boundary.

No USB application behavior, scheduler, motor output, receiver input, sensor access, logging, or flight-control behavior has been implemented.

## Known issues and limitations

- No ST-Link was connected during Milestone 0.2, as confirmed by STM32CubeProgrammer 2.23.0. SWD programming, reset, HSE startup, and debugger-visible runtime values therefore remain physical-board checks.
- The hardware repository has owner changes in its working tree. They were inspected read-only and remain untouched.
- The minimal loop intentionally consumes the processor; it now publishes uptime but remains a temporary bring-up loop that the cooperative scheduler replaces in later milestones.
- No GPIO or routed peripheral listed in the V1 hardware map is initialized by Milestone 0.4. TIM5 runs internally without timer pins or DMA.
- Physical verification of the 1 MHz TIM5 rate and continuous operation through a real 71.58-minute hardware wrap remains pending because no board/ST-Link session was available during implementation.
- Correct 64-bit extension assumes global interrupts are not continuously disabled for a complete 71.58-minute TIM5 wrap period; ordinary bounded critical sections are many orders of magnitude shorter.
- Receiver UART, motor timer/DMA, GPS PPS capture, IMU EXTI, and ADC sampling decisions remain deliberately deferred to their owning milestones.
- Host tests cover the portable overflow resolver; TIM5 register behavior and frequency still require the separate physical-board checks described above.

## Open questions

- Choose an open-source license before declaring the public source licensed for reuse.
- Decide later whether the exact GCC `15.3.1` reproducibility check should become a documented compatible version range; Milestone 0.2 deliberately matches the proven tester toolchain.
- Complete the documented SWD flash/debug checks when the board and ST-Link are connected.
- Confirm the documented LED polarity, WS2812 logic margin, and microSD card-detect polarity during physical validation.

## Next step

After owner review and a separately approved commit, Milestone 0.5 will define the hardware-independent fixed-capacity `Task` abstraction. It will not implement the cooperative scheduler, which remains Milestone 0.6.

## Milestone 0.4 verification

- The host-development build runs one native timebase test executable; all tests pass.
- The tests execute the same snapshot resolver linked into the firmware and cover normal reads, a pending update before interrupt service, an overflow interrupt racing a read, and monotonic values across `0xffffffff` to zero.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 5,956 bytes of Flash and reserves 2,088 bytes of RAM.
- Release uses 3,940 bytes of Flash and reserves 2,088 bytes of RAM.
- The Release ELF is a statically linked, 32-bit little-endian ARM EABI5 hard-float executable with no unresolved symbols.
- The TIM5 vector slot resolves to the implemented Thumb `TIM5_IRQHandler` rather than the startup default handler.
- `time_us`, `mcu_timebase_initialize`, `mcu_timebase_us`, `mcu_timebase_handle_overflow_interrupt`, `timebase_snapshot_resolve`, `TIM5_IRQHandler`, and `firmware_uptime_us` remain inspectable symbols.
- Application source contains no STM32/HAL register access, and Git whitespace validation passes.

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
