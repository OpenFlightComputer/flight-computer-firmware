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
| `SystemCoreClock` | `168000000` |

Intermediate and error values are intentionally observable:

| Value | Meaning |
| ---: | --- |
| 0 | Reset/startup has not reached HAL initialization |
| 1 | Board initialization started |
| 2 | MCU initialization and board clock verification completed |
| 3 | Clock frequency verified; minimal loop running |
| 100 | MCU/HAL initialization failed |
| 101 | HSE/PLL or bus-clock configuration failed |
| 102 | `SystemCoreClock` did not equal 168 MHz |

The loop counter is a temporary bring-up aid, not a scheduler or timing guarantee. No LED is used as a heartbeat because Milestone 0.3 does not initialize board peripherals and the discrete LED hardware still has a documented polarity/connectivity concern.

## Current physical validation status

Debug and Release images build and pass static artifact inspection. STM32CubeProgrammer 2.23.0 was installed but reported no attached ST-Link during the Milestone 0.2 implementation session. Programming, reset, HSE startup, the running status, and the loop counter must therefore be checked when the hardware is connected.
