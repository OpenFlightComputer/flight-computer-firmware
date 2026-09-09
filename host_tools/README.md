# OpenFlightComputer host tools

The `openflightcomputer` package contains reusable host-side services. The
root `./ofc` script is only a command-line adapter; a future GUI can call the
same build, flash, device, protocol, reporting, and smoke-workflow APIs without
capturing terminal output.

Run `./ofc --help` from the repository root. Hardware-changing smoke tests are
safe by default with respect to actuators: they query `status` and `health` but
never send `arm` or motor commands.

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
the active motor directions, propeller layout, mixer factors, and receiver
failsafe policy. Firmware reasserts configured ESC directions with ten DShot
command frames before every arm. Normal flashing preserves the configuration
sector; a mass erase or reset restores the defaults compiled from
`config/default-flight-configuration.json`. See
`docs/flight-configuration.md` for the complete contract.

Run the host test suite from the repository root with:

```bash
uv run --project host_tools --group dev pytest host_tools/tests
```
