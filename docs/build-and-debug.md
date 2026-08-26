# Build, flash, and debug

## Versioned inputs

| Input | Required version |
| --- | --- |
| Target | STM32F405RGT6 on Flight Computer V1 |
| STM32CubeF4 | `v1.28.3`, commit `94cae6e83f00e276a11957e7833c01ac3d0bd7af` |
| CMSIS STM32F4 device package | commit `3c77349ce04c8af401454cc51f85ea9a50e34fc1` |
| STM32F4 HAL driver | commit `b6f0ed3829f3829eb358a2e7417d80bba1a42db7` |
| Arm GNU Toolchain | `15.3.rel1`, GCC `15.3.1`, including newlib |
| CMake | minimum 3.25; tested with 4.4.2 |
| Ninja | tested with 1.13.2 |

The exact compiler check initially matches the proven manufacturing-test build. A future milestone may widen this to a tested compatibility range, but silent toolchain drift is not accepted now.

## VS Code code navigation

The repository configures the clangd language server against the Debug
firmware compilation database. This gives Go to Definition, Find All
References, symbol search, completion, and diagnostics the same target defines,
include paths, and Cortex-M4 compiler flags used by the real firmware build.

Install the recommended clangd and CMake Tools extensions when VS Code prompts.
On macOS, the workspace uses the clangd supplied by the Apple Command Line
Tools. The overlapping STM32 clangd extension is disabled for this workspace
so two language servers do not compete over C files.

After cloning or deleting `build/`, run the default build task once:

```text
Terminal -> Run Build Task...
Firmware: Build Debug
```

The task configures the `firmware-debug` CMake preset before building, which
regenerates `build/firmware-debug/compile_commands.json`. clangd watches that
database and builds its index in the background. Run `clangd: Restart language
server` from the Command Palette after changing toolchains or if an already
open VS Code window does not immediately reload the new workspace settings.

With the index ready, use Command-click or F12 on a symbol such as
`mcu_timebase_initialize`, Shift-F12 for references, and Command-T for symbols
in the current workspace.

## Dependency setup

Initialize the pinned STM32CubeF4 repository and only the two nested dependencies used by this image:

```bash
git submodule update --init firmware/third_party/STM32CubeF4
git -C firmware/third_party/STM32CubeF4 submodule update --init \
  Drivers/CMSIS/Device/ST/STM32F4xx \
  Drivers/STM32F4xx_HAL_Driver
```

Do not use an unrestricted recursive submodule update: STM32CubeF4 contains many unrelated evaluation-board and component repositories.

The toolchain file searches `PATH`, Arm's standard macOS application location, and this development fallback:

```text
~/.local/share/OpenFlightComputer/arm-gnu-toolchain-15.3.rel1
```

A different complete installation can be selected with the `ARM_GNU_TOOLCHAIN_ROOT` CMake cache variable.

## Build

Build Debug firmware for source-level debugging:

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug
```

Build Release firmware for size and optimized-image validation:

```bash
cmake --preset firmware-release
cmake --build --preset firmware-release
```

Each profile writes these files below its `build/<profile>/firmware/` directory:

```text
openflightcomputer-flight-firmware.elf
openflightcomputer-flight-firmware.hex
openflightcomputer-flight-firmware.bin
openflightcomputer-flight-firmware.map
```

The build directory also contains `compile_commands.json` for editor indexing. The ELF is the preferred SWD programming and debugging input because it retains addresses and symbols.

## SWD programming

Flight Computer V1 uses its five-pin SWD connection: 3.3 V reference, SWDIO, SWCLK, NRST, and GND. Connect the board and ST-Link with motor power disconnected for this foundation check.

List probes with STM32CubeProgrammer:

```bash
STM32_Programmer_CLI --list
```

Program and verify the Debug ELF using SWD connect-under-reset at 1 MHz:

```bash
STM32_Programmer_CLI \
  -c port=SWD sn=<ST-LINK-SERIAL> mode=UR freq=1000 \
  -d build/firmware-debug/firmware/openflightcomputer-flight-firmware.elf \
  -v
```

Reset only after verification succeeds:

```bash
STM32_Programmer_CLI \
  -c port=SWD sn=<ST-LINK-SERIAL> mode=UR freq=1000 \
  -rst
```

On this macOS installation the CLI is located inside STM32CubeProgrammer's application bundle rather than on `PATH`; invoking that executable by its full path is equivalent.

## Debugger verification

Launch the Debug ELF through VS Code or another GDB client backed by the ST-Link GDB server. Halt at `main`, then continue and inspect:

| Symbol | Expected value |
| --- | --- |
| `firmware_boot_status` | `BOOT_STATUS_RUNNING` (`3`) |
| `firmware_main_loop_iterations` | Increasing between debugger halts |
| `firmware_uptime_us` | Increasing elapsed microseconds |
| `firmware_fast_task_executions` | Increasing at approximately 1,000 Hz |
| `firmware_medium_task_executions` | Increasing at approximately 100 Hz |
| `firmware_slow_task_executions` | Increasing at approximately 10 Hz |
| `firmware_scheduler_last_result` | `0` while idle or `1` after execution |
| `firmware_system_state_machine.current` | `SYSTEM_STATE_DISARMED` (`2`) |
| `firmware_system_state_machine.previous` | `SYSTEM_STATE_INITIALIZING` (`1`) |
| `firmware_system_state_last_result` | `SYSTEM_STATE_TRANSITION_OK` (`0`) |
| `firmware_fault_system.active_count` | `0` after successful startup |
| `firmware_fault_system.dropped_record_count` | `0` |
| `firmware_fault_last_result` | `0xffffffff` until the first report |
| `SystemCoreClock` | `168000000` |
| `TIM5->PSC` | `83` |
| `TIM5->ARR` | `0xffffffff` |

Intermediate and error values are intentionally observable:

| Value | Meaning |
| ---: | --- |
| 0 | Reset/startup has not reached HAL initialization |
| 1 | Board initialization started |
| 2 | MCU initialization and board clock verification completed |
| 3 | Initialization completed; scheduler running in `DISARMED` state |
| 100 | MCU/HAL initialization failed |
| 101 | HSE/PLL or bus-clock configuration failed |
| 102 | `SystemCoreClock` did not equal 168 MHz |
| 103 | TIM5 timebase parameters or active timer clock were invalid |
| 104 | A diagnostic task could not be registered |
| 105 | Scheduler initialization failed |
| 106 | Scheduler entered an invalid runtime state |
| 107 | An expected application-state transition was rejected |
| 108 | The immutable production fault catalogue could not initialize |
| 109 | The monotonic fault timestamp clock could not be attached |

The task counters validate scheduler ratios without using board GPIO. Halting the core in a debugger also halts callbacks; after continuation, missed periods are skipped and recorded rather than replayed. Fatal paths retain their detailed boot status and populate `firmware_fault_system.records` before halting; faults before timebase startup have timestamp-valid flags cleared. No LED is used as a heartbeat because the discrete LED hardware still has a documented polarity/connectivity concern.

## Current physical validation status

Debug and Release images build and pass static artifact inspection. STM32CubeProgrammer 2.23.0 was installed but reported no attached ST-Link during the Milestone 0.2 implementation session. Programming, reset, HSE startup, the running status, and the loop counter must therefore be checked when the hardware is connected.
