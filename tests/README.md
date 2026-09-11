# Tests

The native `spi_device_tests` target verifies the generic injected transport's
operation validation, forwarding, context preservation, and safe optional
deselect/delay behavior.

The native `bmi270_driver_tests` target replaces the Bosch entry points and
physical SPI backend with fakes. It verifies read/write framing and the SPI
dummy byte, the exact tester-proven accelerometer/gyroscope configuration,
sensor enable order, initialization failure boundaries, raw sample mapping,
and communication diagnostics. Physical SPI3 routing and BMI270 behavior
remain board checks.

The native `imu_sample_tests` target verifies signed-permutation body-axis
mapping, safe handling of the full signed 16-bit range, invalid mapping
rejection, and the exact fresh/stale/lost boundaries including clock rollback.

The native `imu_service_tests` target uses an injected source and clock to
verify one-read-per-run boundedness, complete owned publication, timestamps,
saturating sequence/statistics, last-good-sample retention, startup
unavailable/lost behavior, recovery after source errors, and invalid
initialization rejection.

The native `timebase_snapshot_tests` target verifies normal reads, pending
hardware overflows, interrupt races, and monotonic behavior across a 32-bit
counter wrap.

The native `task_tests` target verifies fixed-capacity registration, validation,
unique names, deterministic registration order, priority/period preservation,
and zero-initialized scheduling and execution metadata without invoking task
callbacks.

The native `receiver_service_tests` target uses an injected decoded-frame source
and monotonic clock to verify initialization, one-read-per-service boundedness,
owned timestamped snapshots, replacement sequencing, preservation after invalid
or error results, atomic normalized publication, freshness advancement,
scheduler-callback behavior, and saturating statistics. It does not test CRSF
parsing or physical UART reception.

The native `receiver_normalization_tests` target verifies copied and rejected
configuration, the replaceable default profile, asymmetric centered-axis
scaling, throttle scaling, clamping, reversal, switch interpretation, and exact
timestamp/sequence preservation.

The native `receiver_freshness_tests` target verifies configuration bounds,
unavailable data, inclusive fresh/lost boundaries, stale and lost transitions,
clock rollback, and stable diagnostic names.

The native `receiver_arming_tests` target verifies startup-low qualification,
fresh/live input requirements, one-shot low-to-high arming, low-throttle
admission, mandatory retoggle after rejection, receiver-owned disarming, USB
source isolation, and rejected lifecycle operations. It uses fake common
motor-control entry points and never constructs or submits a motor command.

The native `crsf_tests` target verifies the tester-derived CRC-8/DVB-S2
implementation, packed 16-channel decoding, signed link statistics, malformed
frame accounting, parser recovery, and invalid-argument handling.

The native `crsf_receiver_source_tests` target verifies byte-stream injection,
link-statistics retention, caller-owned channel publication, malformed and
hardware-error outcomes, valid-frame precedence, and the 512-byte per-call
processing bound used to protect the cooperative scheduler.

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

The native `usb_imu_response_tests` target verifies available and unavailable
IMU schemas, signed mapped axes, full-width sequence/age formatting, task/load
diagnostics, bounded output, and invalid arguments.

The native `usb_command_processor_tests` target uses a fake line source and
transport to verify status/structured-health responses, health-aware arm
admission, state-machine arm/disarm transitions, malformed/unsupported errors,
all four manual motor selectors, the full normalized throttle range, and
pending-response backpressure. It also verifies request-only IMU snapshot reads
and active-flight rejection. The STM32 USB device library, interrupt behavior,
pins, enumeration, and physical transfer remain firmware-build or board-level
checks.

The native `foundation_integration_tests` target verifies complete
state/fault/health chains for successful and degraded startup, fatal startup,
ordinary and critical armed-runtime faults, recovery, and registry exhaustion.

The native `motor_safety_policy_tests` target verifies all health outcomes:
`OK`, `WARNING`, and `DEGRADED` permit arm/output consideration, while
`UNKNOWN`, `CRITICAL`, and invalid values fail closed.

The native `motor_control_tests` target uses an injected copying backend to
verify fail-closed initialization, mandatory initial stop, singleton ownership,
private mapping, state/health/validity/freshness gates, command acceptance
without immediate output, retained-command replacement, periodic 1 kHz-style
retransmission, inclusive lease enforcement, stuck-transfer detection,
failsafe entry,
critical backend faults, and force-stop uncertainty.

The motor architecture check runs in every normal build and as the named
`motor_architecture` CTest. It scans production sources and rejects raw
motor-output/DShot calls or new arm-event sites outside their explicit owner
files. It guards against accidental bypass; it is not a security boundary
against modified source code.

The native `motor_command_tests` target verifies invalid initialization,
atomic four-motor creation, normalized boundaries, exact-stop
canonicalization, all-zero stop commands, NaN/infinity/out-of-range rejection
without replacement, invalidation, timeout boundaries, future timestamps, and
freshness near the 64-bit time limit.

The native `motor_configuration_tests` target verifies per-motor direction
validation, invalid configurations, and stable direction names.
`board_flight_configuration_storage_tests` verifies the complete versioned
payload, prior motor-direction payload migration, valid load/save/clear
behavior, corrupt or incompatible data rejection, and board-storage error
propagation. `flight_configuration_service_tests` covers source selection,
disarmed-only whole-document replacement/reset, persistence ordering, and
runtime application. `quad_x_mixer_tests` and `flight_control_tests` cover the
stabilized mix equations, zero-throttle fast path, proportional saturation,
collective shifting, IMU freshness gates, authority, submission, and explicit
receiver-loss failsafe entry.

The native `motor_mapping_tests` target verifies identity defaults, exhaustively
classifies all 256 in-range assignments, rejects configuration unless the
disarmed condition is supplied, preserves mappings atomically on
failure, reorders complete logical-to-physical commands, supports in-place use,
and rejects corrupted commands and mappings.

The native `motor_output_tests` target uses an injected fake backend to verify
complete callback validation, backend and initial-stop failure handling,
descriptor copying, command revalidation/canonicalization, distinct facade
storage, accepted-copy lifetime, busy/error/unknown mapping, and force-stop's
accepted-or-error contract. It also verifies complete direction-set
validation/result mapping and asynchronous backend status mapping.

The native `dshot_motor_backend_tests` target replaces the V1 board engine with
a fake and verifies DShot300 initialization, normalized-throttle conversion,
exact physical `M1`-through-`M4` table order, caller-independent copying, busy
behavior, exact direction command 20/21 frames with the request bit, all-zero
force-stop storage, board result/status error mapping, and compact board
failure-context propagation.

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
plus the physical-order-to-CCR-order row transformation and the selected
168 MHz TIM8 update DMA2 Stream 1/Channel 7 four-register burst. It proves
recorded configuration and ordering logic, not physical routing or output.

The native `board_usb_tests` target verifies the explicit VBUS-mode semantics
and that Flight Computer V1 selects assume-present behavior rather than its
defective PA9 sensing path.

The native `uint64_decimal_tests` target verifies zero, decimal boundaries,
`UINT64_MAX`, bounded zero padding, invalid arguments, and exact-capacity
rejection without any formatted long-long I/O.

The native IMU targets cover sample publication, stationary gyro calibration,
first-order gyro filtering, accelerometer angle calculation, timestamp-derived
complementary estimation, duplicate/stale/gap handling, and invalid
configuration. The controller targets additionally cover bounded angle-to-rate
conversion, yaw-rate passthrough, PID continuity, zero-throttle reset, and the
stabilized flight-control integration, including the seed-stop cycle and lost
IMU failsafe. Controller outputs now have receiver-path motor authority and
still require physical validation.

Hardware tests remain separate and must not be represented as passing host tests.
