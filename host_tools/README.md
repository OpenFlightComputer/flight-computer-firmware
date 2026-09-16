# OpenFlightComputer host tools

The `openflightcomputer` package contains reusable host-side services. The
root `./ofc` script is only a command-line adapter; a future GUI can call the
same build, flash, device, protocol, reporting, and smoke-workflow APIs without
capturing terminal output.

Run `./ofc --help` from the repository root. Hardware-changing smoke tests are
safe by default with respect to actuators: they query `status` and `health` but
never send `arm` or motor commands.

Update a normally running, disarmed flight computer through USB-C with:

```bash
./ofc firmware flash-usb --profile release
```

This asks firmware to stop motor output and reset once into the STM32F405 ROM
DFU loader, programs and verifies only application flash, restarts it, and
confirms the running build ID. It preserves the configuration sector and never
requests a mass erase. SWD remains the recovery path. See
`docs/usb-firmware-update.md`.

Inspect the receiver once or use the tester-style live display with:

```bash
./ofc device receiver
./ofc device receiver --watch
./ofc device receiver --watch --interval 0.2
```

The display includes all 16 raw channels, session-local minima/maxima,
normalized flight controls, freshness/failsafe state, link statistics, and
UART/parser/DMA diagnostics. Each refresh is an on-demand USB request; it does
not alter the firmware's independent receiver task or motor state.

Inspect the latest mapped BMI270 sample once or continuously with:

```bash
./ofc device imu
./ofc device imu --watch
./ofc device imu --watch --interval 0.2
./ofc device imu calibrate-level
```

The firmware only copies its existing coherent sample on request; it does not
read the sensor again or add a visualization task. The host converts raw counts
to g and degrees per second and draws independent bars for the two ranges.
Inspection is disarmed-only, and watch polling cannot be configured faster than
10 Hz, so the diagnostic path cannot run during active flight.

`calibrate-level` requires the flight computer to be disarmed. Place the fully
mounted aircraft level and keep it still; firmware collects fresh acceleration
and gyro samples for the configured duration, rejects movement and implausible
gravity, then atomically persists roll/pitch mounting trim in the unified
configuration. Arming remains blocked until a calibration has succeeded. A
failed attempt preserves the previous valid trim.

Inspect the complete control pipeline with a terminal attitude horizon,
desired/measured rates, PID terms, Quad-X motor outputs, and trace health:

```bash
./ofc device control --watch
./ofc device control --watch --level low
./ofc device control --watch --level full --output control-trace.json
./ofc device control --watch --output control-trace.csv
```

Every watch writes a uniquely named JSON trace by default, including when
Ctrl-C stops the dashboard. Use `--output` to choose another base path or CSV.

The requested output is a base name. Each export receives a versioned,
collision-resistant name such as `control-trace-v1-a1b2c3d4.json` and embeds
the firmware identity, active configuration, capture settings, schema
versions, and creation time needed to compare it with later runs.

Trace capture must start while disarmed but may continue while armed. It
preserves the terminal sample and stops automatically after disarm, failsafe,
or fault. Animation and long history live on the host; firmware uses a fixed
non-blocking RAM ring and reports any dropped records. See
`docs/control-diagnostics.md` for the capture contract.

The separately explicit propeller-free bench path is:

```bash
./ofc device arm
./ofc motor run --motor 1 --throttle 0.10 --duration 5
```

The first command only changes lifecycle state. The second selects one of four
logical motors and accepts the command model's full normalized range above the
0.001 stop threshold through 1.0. Duration must be positive and finite. It
first sends five seconds of zero frames, refreshes a 100 ms firmware lease
during active output, and always attempts zero-output and disarm cleanup. See
`docs/usb-json-protocol.md` for the complete contract.
The host refresh rate renews command authority; the firmware's 1 kHz motor task
independently repeats the retained command as DShot frames.
The motor workflow prints `PREPARING`, `ACTIVE`, `CLEANUP`, and `DISARMED`
boundaries so observed tones and movement can be assigned to the correct phase.
Serial reads return as soon as a complete newline-delimited response arrives;
they do not delay the next lease refresh while waiting to fill a large buffer.

The complete flight configuration is managed as one portable JSON document:

```bash
./ofc config read --output quad.json
./ofc config write quad.json
./ofc config reset
```

Read and write always transfer the whole configuration; there are no per-field
mutation commands. Writes and reset are disarmed-only and atomically replace
the active motor directions, propeller layout, control pipeline, IMU policy,
level trim, and receiver failsafe policy. Firmware reasserts configured ESC
directions with ten DShot command frames before every arm. Normal flashing
preserves the configuration sector; a mass erase or reset restores the defaults compiled from
`config/default-flight-configuration.json`. See
`docs/flight-configuration.md` for the complete contract.

Download the automatic SD-card flight blackbox without removing the card:

```bash
./ofc storage status
./ofc storage initialize --yes
./ofc flight-log list
./ofc flight-log download latest --json-output latest-flight.json
```

The initialize operation replaces the raw blackbox index and therefore needs
the explicit `--yes` confirmation. See `docs/blackbox.md` for the recording
and versioning contract.

Run the host test suite from the repository root with:

```bash
uv run --project host_tools --group dev pytest host_tools/tests
```
