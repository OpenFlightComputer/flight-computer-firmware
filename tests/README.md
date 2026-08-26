# Tests

The native `timebase_snapshot_tests` target verifies normal reads, pending
hardware overflows, interrupt races, and monotonic behavior across a 32-bit
counter wrap.

The native `task_tests` target verifies fixed-capacity registration, validation,
unique names, deterministic registration order, priority/period preservation,
and zero-initialized scheduling and execution metadata without invoking task
callbacks.

The native `scheduler_tests` target uses a fake microsecond clock to verify
ready-task selection, batch fairness, disabled tasks, stable periodic release,
execution measurements, skipped-release behavior, and overrun detection.

Future hardware-independent tests will cover task registration, scheduling,
state transitions, faults, logging queues, protocol parsing, DShot encoding,
receiver parsing, mixing, and control algorithms as their milestones are
approved.

Hardware tests remain separate and must not be represented as passing host tests.
