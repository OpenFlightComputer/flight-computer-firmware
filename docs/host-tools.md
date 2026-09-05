# OpenFlightComputer host CLI

`./ofc` is the single host entry point for firmware development. It uses `uv`
to run the Python package in `host_tools/` and currently provides:

```text
ofc firmware build [--profile debug|release]
ofc firmware flash [--profile debug|release] [--firmware IMAGE.elf]
ofc device status [--port PATH]
ofc device arm [--port PATH]
ofc device disarm [--port PATH]
ofc device monitor [--port PATH]
ofc motor run --motor 1 --throttle VALUE --duration SECONDS [--port PATH]
ofc smoke [--profile debug|release] [--no-flash]
```

The default profile is Release. `firmware flash` and the default `smoke`
workflow build the selected profile unless `--firmware` supplies another ELF.
They require exactly one ST-Link or an explicit `--probe-serial`, program and
read-back verify through STM32CubeProgrammer, then reset the target. The
programmer can be selected with `--programmer` or
`STM32CUBE_PROGRAMMER_CLI`.

USB discovery selects the flight-firmware development identity `CAFE:4002` or
an explicit `--port`. `device status` makes one correlated request while
ignoring interleaved log events. `device monitor` emits the live newline JSON
stream until interrupted.

`device arm` and `device disarm` expose the existing lifecycle commands without
combining them with output. `motor run` is a separate propeller-free bench
workflow and refuses to arm implicitly. Both the host and firmware restrict it
to logical motor 1 and at most 10% normalized throttle; the host additionally
bounds one run to one second and requires an active request above the command
model's 0.001 stop threshold. The workflow prepares the ESC with zero commands,
refreshes the firmware's 100 ms command lease, and attempts repeated zero plus
disarm cleanup after completion, an error, or Ctrl-C. The firmware lease is the
independent stop mechanism if host cleanup cannot reach the board.

## Smoke safety and reports

The smoke workflow sends only `status` and `health`; it never sends `arm`,
`disarm`, or motor commands. It requires `DISARMED`, `OK` health, complete
fault data, zero dropped fault records, and a nonempty identity. When it built
the flashed artifact itself, it also requires the running version/build ID to
match that artifact.

Every completed smoke run writes a JSON report under `reports/` by default.
Use `--report PATH` to choose another location or `--json` for machine-readable
standard output. Reports and local Python environments are ignored by Git.

## Frontend boundary

The CLI only parses arguments and presents results. The modules under
`host_tools/openflightcomputer/` own reusable services and return dataclasses
or dictionaries rather than printing or prompting. Progress is delivered as
callbacks. A future GUI can therefore call the same build, programmer, USB,
protocol, reporting, and workflow APIs without invoking the CLI as a child
process.

This is intentionally not a plugin system, background daemon, database, or
configuration protocol. Those can be added behind the same service boundaries
when an actual frontend or configuration milestone requires them.
