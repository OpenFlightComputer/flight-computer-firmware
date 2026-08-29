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

The native `logging_tests` target verifies defaults and names, filtering and
module overrides, suppression of filtered argument evaluation, timestamps and
sequences, bounded formatting and truncation, canonical lines, FIFO wrap and
overflow, backend retry/error behavior, invalid operations, and saturating
statistics.

The native `usb_logging_backend_tests` target replaces the physical transport
with a copying fake and verifies exact JSON bytes and escaping, accepted
ownership, busy retry/retention, and error/drop mapping.

The native `newline_framer_tests` target verifies fragmented input, LF and
CRLF, empty/multiple lines, the exact 256-byte bound, oversized-line discard,
recovery, explicit discard, and saturating overflow statistics.

The native `usb_json_protocol_tests` target verifies every command, member
ordering, unsupported commands, strict rejection of malformed/noncanonical
objects, and exact bounded response serialization.

The native `usb_command_processor_tests` target uses a fake line source and
transport to verify status/health summaries, state-machine-only arm/disarm,
rejected transitions, malformed/unsupported errors, and pending-response
backpressure. The STM32 USB device library, interrupt behavior, pins,
enumeration, and physical transfer remain firmware-build or board-level checks.

Future hardware-independent tests will cover structured health detail, DShot
encoding, receiver parsing, mixing, and control algorithms as their milestones
are approved.

Hardware tests remain separate and must not be represented as passing host tests.
