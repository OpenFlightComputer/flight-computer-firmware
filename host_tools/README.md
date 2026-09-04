# OpenFlightComputer host tools

The `openflightcomputer` package contains reusable host-side services. The
root `./ofc` script is only a command-line adapter; a future GUI can call the
same build, flash, device, protocol, reporting, and smoke-workflow APIs without
capturing terminal output.

Run `./ofc --help` from the repository root. Hardware-changing smoke tests are
safe by default with respect to actuators: they query `status` and `health` but
never send `arm` or motor commands.

Run the host test suite from the repository root with:

```bash
uv run --project host_tools --group dev pytest host_tools/tests
```
