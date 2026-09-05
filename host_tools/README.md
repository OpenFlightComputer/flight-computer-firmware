# OpenFlightComputer host tools

The `openflightcomputer` package contains reusable host-side services. The
root `./ofc` script is only a command-line adapter; a future GUI can call the
same build, flash, device, protocol, reporting, and smoke-workflow APIs without
capturing terminal output.

Run `./ofc --help` from the repository root. Hardware-changing smoke tests are
safe by default with respect to actuators: they query `status` and `health` but
never send `arm` or motor commands.

The separately explicit propeller-free bench path is:

```bash
./ofc device arm
./ofc motor run --motor 1 --throttle 0.02 --duration 0.25
```

The first command only changes lifecycle state. The second is hard-limited by
both host and firmware to motor 1 and at most 10% throttle. Active host requests
must be above the command model's 0.001 stop threshold and last at most one
second. It refreshes a 100 ms firmware lease and always attempts zero-output
and disarm cleanup. See `docs/usb-json-protocol.md` for the complete contract.

Run the host test suite from the repository root with:

```bash
uv run --project host_tools --group dev pytest host_tools/tests
```
