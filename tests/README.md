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

The native `system_state_tests` target verifies all 36 state/event pairs,
initialization, explicit-only arming, synchronous disarm, failsafe recovery,
terminal faults, invalid inputs, previous-state preservation, and saturating
statistics.

The native `fault_tests` target verifies catalogue validation, warning, fault,
and critical handling, critical transitions from every lifecycle state,
timestamp validity, repeated records, context, clearing/latching, capacity
exhaustion, critical safety under overflow, slot reuse, invalid operations, and
saturating statistics.

Future hardware-independent tests will cover logging queues, protocol parsing,
DShot encoding, receiver parsing, mixing, and control algorithms as their
milestones are approved.

Hardware tests remain separate and must not be represented as passing host tests.
