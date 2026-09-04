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

The native `health_tests` target verifies overall-state precedence, severity
counts, recoverable clearing, critical lifecycle override, defensive
incomplete-registry `UNKNOWN` handling, terminal public-path overflow, invalid
records, and stable health names.

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

The native `usb_health_response_tests` target verifies exact empty health,
validity-aware active-fault metadata, fixed-capacity response truncation,
reported/active count distinction, and invalid destination handling.

The native `usb_command_processor_tests` target uses a fake line source and
transport to verify status/structured-health responses, state-machine-only arm/disarm,
rejected transitions, malformed/unsupported errors, and pending-response
backpressure. The STM32 USB device library, interrupt behavior, pins,
enumeration, and physical transfer remain firmware-build or board-level checks.

The native `foundation_integration_tests` target verifies complete
state/fault/health chains for successful and degraded startup, fatal startup,
ordinary and critical armed-runtime faults, recovery, and registry exhaustion.

The native `motor_command_tests` target verifies invalid initialization,
atomic four-motor creation, normalized boundaries, exact-stop
canonicalization, all-zero stop commands, NaN/infinity/out-of-range rejection
without replacement, invalidation, timeout boundaries, future timestamps, and
freshness near the 64-bit time limit.

The native `motor_mapping_tests` target verifies identity defaults, exhaustively
classifies all 256 in-range assignments, rejects configuration unless both
disarmed and stopped conditions are supplied, preserves mappings atomically on
failure, reorders complete logical-to-physical commands, supports in-place use,
and rejects corrupted commands and mappings.

The native `motor_output_tests` target uses an injected fake backend to verify
complete callback validation, backend and initial-stop failure handling,
descriptor copying, command revalidation/canonicalization, distinct facade
storage, accepted-copy lifetime, busy/error/unknown mapping, and force-stop's
accepted-or-error contract.

The native `dshot_encoder_tests` target exhaustively covers all 2,048 protocol
values with telemetry both clear and set, independently verifies the checksum
and recovered fields, and enforces the public stop/throttle versus command
separation. It proves frame construction only, not a physical waveform.

The native `dshot_timing_tests` target verifies the exact V1 DShot300 timing,
MSB-first compare conversion, every bit and timer lane, four-lane interleaving,
the documented mixed 25%/50% CCR table, trailing-low slots, unsupported
DShot600 rejection, corrupted-profile rejection, overlapping source/destination
storage, and atomic failure behavior. It proves buffer representation only,
not timer/DMA or physical output.

The native `board_motor_output_map_tests` target verifies the fixed V1
`ESC_M1` through `ESC_M4` mapping to PC9 through PC6 and TIM8_CH4 through CH1,
plus the selected 168 MHz TIM8 update DMA2 Stream 1/Channel 7 four-register
burst. It proves recorded configuration data, not physical routing or output.

The native `board_usb_tests` target verifies the explicit VBUS-mode semantics
and that Flight Computer V1 selects assume-present behavior rather than its
defective PA9 sensing path.

The native `uint64_decimal_tests` target verifies zero, decimal boundaries,
`UINT64_MAX`, bounded zero padding, invalid arguments, and exact-capacity
rejection without any formatted long-long I/O.

Future hardware-independent tests will cover receiver parsing, mixing, and
control algorithms as their milestones are approved.

Hardware tests remain separate and must not be represented as passing host tests.
