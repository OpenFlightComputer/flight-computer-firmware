# Tests

The native `timebase_snapshot_tests` target verifies normal reads, pending
hardware overflows, interrupt races, and monotonic behavior across a 32-bit
counter wrap.

Future hardware-independent tests will cover task registration, scheduling,
state transitions, faults, logging queues, protocol parsing, DShot encoding,
receiver parsing, mixing, and control algorithms as their milestones are
approved.

Hardware tests remain separate and must not be represented as passing host tests.
