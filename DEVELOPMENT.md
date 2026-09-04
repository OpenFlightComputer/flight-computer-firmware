# Development status

## Current phase

Phase 1 — DShot actuator subsystem.

## Current milestone

Milestone 1.6 — DShot300 timing and interleaved DMA-buffer representation:
**implementation and host verification complete; awaiting owner review**.

## Last completed milestone

Milestone 1.5 — TIM8/GPIO/DMA board mapping. Fixed physical routes, grouped
DMA resources, and the safely configurable logical assignment are documented,
host-tested, reviewed, committed, and integrated.

## Current implementation status

- Added the generic `board.h` contract selected by the firmware build.
- Added Flight Computer V1 board support owning board identity, manufacturing/schematic revision, expected clock, initialization policy, and translation of MCU failures into board-level results.
- Added a narrow STM32F405 MCU API owning HAL initialization, clock setup, core-frequency access, and interrupt-disabled halt behavior.
- Refactored `app/main.c` to use only the board API; application code no longer includes STM32 HAL/CMSIS or directly interprets MCU behavior.
- Retained the proven 16 MHz HSE to 168 MHz system-clock implementation and debugger-visible boot/loop behavior.
- Added hardware-layer responsibility documentation at each directory boundary.
- Revalidated the current KiCad project hashes and cross-checked all V1 signal mappings against the manufacturing-test board definition and low-level sources.
- Added `docs/flightcomputer-v1-hardware.md` with installed hardware, full MCU signal mapping, resolved interface selections, deferred choices, and retained electrical concerns.
- Added the hardware-neutral `uint64_t time_us(void)` API and integrated it into the temporary main loop as the debugger-visible `firmware_uptime_us` value.
- Reserved internal TIM5 for the V1 timebase, validated its 84 MHz APB1-derived input clock, and configured a 1 MHz free-running 32-bit counter with no GPIO or DMA use.
- Extended TIM5 to 64 bits with a short update interrupt, a software overflow word, update-pending detection, and a retrying snapshot resolver that handles an interrupt racing a read.
- Added native tests for ordinary reads, pending hardware overflow, interrupt/read races, and monotonic progression across the 32-bit boundary.
- Added `docs/timebase.md` with the clock calculation, initialization contract, overflow strategy, interrupt-priority rationale, guarantees, and physical verification boundary.
- Added a hardware-independent task definition containing name, microsecond period, 8-bit priority, callback/context, scheduling metadata, execution measurements, and enabled state.
- Defined task priority as `0` highest through `255` lowest, with named reference levels and preserved intermediate values.
- Added a fixed-capacity 16-task registry with bounded name validation, unique-name enforcement, deterministic registration order, and no runtime allocation.
- Added host tests for valid metadata initialization, invalid definitions, duplicate names, priority/order preservation, indexed access, and capacity exhaustion.
- Added `docs/task-model.md` describing ownership, lifetime, period, priority, registry, metadata, and scheduler boundaries.
- Added an allocation-free cooperative scheduler driven by an injected monotonic clock; production uses `time_us()` and host tests use a fake clock.
- Added deterministic ready-task selection by priority, release time, and registration order.
- Added ready batches so a task can execute at most once per captured batch, preventing an always-ready high-priority task from repeatedly displacing lower-priority tasks already ready.
- Added phase-preserving release advancement, skipped-release accounting, callback execution/max timing, execution counts, and duration-based overrun detection with saturating statistics.
- Replaced the temporary counter-only loop with scheduler steps and three debugger-visible diagnostic tasks at 1,000 Hz, 100 Hz, and 10 Hz.
- Added host tests for initialization, immediate release, idle behavior, priority/release/order selection, disabled tasks, timing, skipped catch-up, overruns, and high-priority starvation protection.
- Added `docs/scheduler.md` with the algorithm, fairness limits, overload behavior, missed-period policy, diagnostic integration, and explicit RTOS mapping.
- Added a hardware-independent event-driven application state machine covering `BOOT`, `INITIALIZING`, `DISARMED`, `ARMED`, `FAILSAFE`, and terminal `FAULT` states.
- Defined and enforced every legal state/event pair, synchronous disarm transitions, explicit-only arming, failsafe recovery through disarm, and fault entry from every non-fault state.
- Added saturating accepted/rejected transition statistics while preserving current and previous state across rejected events.
- Integrated startup lifecycle transitions so initialization begins in `INITIALIZING`, successful startup enters `DISARMED`, and every existing fatal stop path first enters `FAULT`.
- Added exhaustive host coverage of the 36 state/event combinations, invalid arguments, initialization, terminal-fault behavior, and counter saturation.
- Added `docs/safety.md` documenting lifecycle authority, transition policy, future final actuator gating, fault-system boundaries, and concurrency assumptions.
- Added a fixed-capacity fault system with catalogue-owned IDs, severity, and source so report sites cannot select their own safety response.
- Added 16 active record slots containing first/latest timestamps and validity, latest optional context, saturating occurrence counts, and active state.
- Implemented warning and ordinary-fault recording without lifecycle changes, clearable recoverable records, and reset-latched critical records.
- Connected critical reports synchronously to `SYSTEM_STATE_FAULT`, while preserving the record first and still applying the transition when record capacity is exhausted.
- Added explicit pre-timebase timestamp unavailability and attached `time_us()` only after successful board initialization.
- Mapped every existing fatal board, task, scheduler, state, and fault-clock path to an immutable production fault catalogue and retained detailed boot statuses.
- Added host tests for catalogue validation, all severities, every critical source state, startup arm prevention, timestamps, duplicate coalescing, context, clearing, capacity/slot reuse, invalid operations, and saturation.
- Added `docs/fault-system.md` describing classification ownership, lifecycle effects, diagnostics, overflow safety, startup time, and concurrency boundaries.
- Added a central logging facade with `DEBUG`, `INFO`, `WARN`, `ERROR`, and `FATAL` levels plus fixed current-subsystem module IDs.
- Added a global `INFO` threshold, per-module overrides including `OFF`, and macros that avoid evaluating filtered format arguments.
- Added a 32-record FIFO containing immediate timestamp/validity, 64-bit sequence, level, module, bounded 95-character message, length, and truncation metadata.
- Added bounded `vsnprintf()` producer formatting, FIFO wrap handling, total/per-level drop counters, and saturating filter, truncation, format, drain, busy, and backend-error statistics.
- Added a destination-neutral backend contract and one-record `logging_drain_once()` operation with explicit accepted, busy/retry, and error/drop behavior.
- Added a bounded canonical text formatter containing timestamp, sequence, level, module, message, and newline while keeping transport outside the core.
- Initialized logging before board bring-up, attached `time_us()` only after successful board initialization, and added six debugger-visible startup records plus fatal-path records.
- Added a real non-critical logging-clock fault policy: timestamp degradation is recorded without forcing `SYSTEM_STATE_FAULT`.
- Added host tests for defaults/names, filtering, per-module overrides, filtered-argument evaluation, time/sequence capture, truncation, canonical formatting, FIFO wrap, overflow, backend behavior, invalid inputs, and saturating statistics.
- Added `docs/logging.md` defining levels, modules, formatting, queue/overflow, backend, timing, concurrency, debugging, and no-immediate-output policies.
- Reinspected and closely adapted the manufacturing tester's proven PA9/PA11/PA12, OTG FS, FIFO, static-allocation, STM32 USB Device CDC, descriptor, and interrupt implementation.
- Added a flight-firmware USB identity with development-only `0xCAFE:0x4002`; CMake refuses to label that default identity as distribution-approved.
- Added a two-entry, 160-byte-per-entry USB-owned transmit queue whose accepted result guarantees the complete canonical log line has been copied before the logging record is released.
- Kept USB transfer completion in interrupt context to a flag update; formatting, queue ownership changes, and transfer start/retry run in cooperative-task context.
- Added a production USB logging adapter that maps accepted, busy, and error transport outcomes directly to the Milestone 0.9 backend contract.
- Registered a `logging-drain` task at a 1,000 us period and `TASK_PRIORITY_BACKGROUND`; it advances USB once and attempts at most one record per invocation.
- Added the non-critical `FAULT_ID_USB_LOGGING_INITIALIZATION` policy. USB initialization or backend attachment failure disables USB draining without preventing startup into `DISARMED`.
- Added debugger-visible USB initialization, drain result, and drain execution values plus transport counters for queued/completed/busy/invalid/start-error/ignored-receive behavior.
- Added host tests for exact USB backend bytes, accepted ownership, busy retry, and error/drop behavior.
- Added `docs/usb-cdc-logging.md` defining proven reuse, ownership, scheduling, disconnect, failure, receive, identity, and physical-verification boundaries.

- Closely adapted the tester's allocation-free newline framer, JSMN parser,
  512-byte raw receive ring, two-entry completed-line queue, and short receive
  callback while keeping tester session/application policy out of this firmware.
- Added a 256-byte maximum input line and a 64-byte receive-processing budget
  per 1 ms service release, with discard-through-newline recovery and saturating
  raw-byte, completed-line, and oversized-line drop statistics.
- Added strict request-ID-bearing JSON requests for `status`, `health`, `arm`, and
  `disarm`, with exact bounded response serialization and explicit malformed
  and unsupported-command errors.
- Added an application command processor that handles at most one command per
  release, routes arm/disarm exclusively through the state machine, logs
  accepted transitions, and retains one response across USB backpressure.
- Kept Milestone 0.11 health deliberately limited to lifecycle state plus active
  and dropped fault counts; structured per-fault reporting remains Milestone
  0.12.
- Replaced USB plain-text log output with JSON log objects on the same
  newline-delimited stream, including valid/null timestamp, sequence, level,
  module, escaped message, and truncation flag.
- Enlarged the two USB transmit entries to 768 bytes so each can own a complete
  worst-case escaped log event; response admission is attempted before log
  draining.
- Replaced `logging-drain` with a shared 1,000 Hz background `usb-service` task
  performing bounded receive/TX progress, one command/response action, and one
  logging drain attempt in that order.
- Added non-critical `FAULT_ID_USB_COMMAND_INITIALIZATION`, debugger-visible
  command/service results, and focused host tests for framing, protocol,
  dispatch, transitions, backpressure, exact JSON, and escaping.
- Added `docs/usb-json-protocol.md` defining schemas, lifecycle effects,
  capacity/budget choices, priority, overflow recovery, reuse, and physical
  verification boundaries.
- Added a transport-independent health evaluator that scans the fixed active
  fault registry once and derives `OK`, `WARNING`, `DEGRADED`, `UNKNOWN`, or
  `CRITICAL` using the reviewed precedence.
- Made lifecycle `FAULT` and active critical records override all lower health
  outcomes; defensive inconsistent snapshots can still yield `UNKNOWN`.
- Added retained active, warning, ordinary-fault, critical, and dropped counts
  plus an explicit `fault_data_complete` result without mutating fault or state
  authorities.
- Added stable fault-severity and fault-source names owned by the fault module.
- Replaced the Milestone 0.11 basic health response with per-fault ID, severity,
  source, occurrence count, validity-aware first/latest timestamps, and optional
  raw context.
- Added deterministic fixed-capacity response packing in active-record slot
  order. Only complete records are emitted; `reported_fault_count` and
  `truncated` distinguish USB response omission from internal registry loss.
- Kept health evaluation/serialization in cooperative main context, reused the
  existing 768-byte response buffer and transport backpressure behavior, and
  added no allocation, peripheral access, state mutation, or new scheduled task.
- Added dedicated health-policy, response-serialization, and updated command
  integration tests plus `docs/health-reporting.md`.
- Required a canonical unsigned 32-bit `request_id` on every command, echoed it
  on every valid-envelope response, and used `null` for malformed envelopes.
- Classified fault-registry exhaustion as a critical diagnostic-integrity
  failure that synchronously enters terminal `FAULT`; repeated active IDs still
  reuse their existing slots.
- Audited USB interrupt/main ownership, retained one shared bounded service
  task, added transport-capacity assertions, and removed reset logic coupled to
  literal queue depths.
- Added foundation integration tests spanning startup, armed runtime faults,
  recovery, health projection, and overflow behavior.
- Added the consolidated Phase 0 review and a hardware validation checklist for
  USB overload/recovery, execution time, stack use, timebase, and lifecycle
  checks on the flight image. Boards are now available; the checks remain open.
- Added the hardware-independent `flight/actuators` boundary and a four-motor
  `motor_command_t` containing normalized float throttles, a monotonic timestamp,
  and explicit validity.
- Added atomic command creation that validates all four values before replacing
  the destination, rejects NaN, infinity, and values outside `[0.0f, 1.0f]`, and
  leaves the prior timestamp unchanged after rejection.
- Canonicalized the inclusive `0.001f` stop threshold to exact zero so tiny
  floating-point residue cannot later become a nonzero DShot throttle.
- Added explicit initialization/invalidation and configurable freshness checks
  that reject zero timeouts, future timestamps, invalid commands, and elapsed
  times beyond the inclusive timeout boundary.
- Added compile-time requirements for IEEE-754-compatible 32-bit single
  precision, matching the STM32F405 hard-float build, with no dynamic memory,
  global command, transport, DShot, or hardware dependency.
- Added comprehensive native tests and `docs/motor-command.md` covering the
  Phase 1.1 representation, rejection, freshness, floating-point, and ownership
  contracts.
- Added an instance-based `motor_output_t` facade with injected initialize,
  complete-command submit, and force-stop backend callbacks, without choosing a
  physical output technology.
- Required backend initialization followed by an independently accepted initial
  force-stop before the facade becomes usable; failures and unknown results
  leave it uninitialized.
- Copied the successfully initialized backend descriptor while retaining only
  its explicitly lifetime-bound opaque context pointer.
- Revalidated each public command into facade-owned canonical storage before
  submission, so the backend never receives the caller's pointer and manually
  modified NaN, infinity, range, or near-zero values cannot bypass the command
  contract.
- Defined accepted submission to require a synchronous backend-owned copy,
  while busy/error results retain nothing. Complete four-motor snapshots avoid
  partially updated motor sets.
- Kept force-stop separate from `motor_command_t` with accepted-or-error backend
  semantics: it cannot report normal backpressure and must override pending
  demand within each future backend.
- Added fake-backend host tests and `docs/motor-output.md` covering initialization,
  descriptor/context lifetime, caller/backend storage ownership, result mapping,
  complete-command submission, and force-stop limits.
- Kept the future lower DShot peripheral independent of flight types: an
  application-owned adapter will implement this backend contract while calling
  the selected peripheral API.
- Incorporated the completed V1 manufacturing acceptance evidence into the
  roadmap without treating tester success as flight-image validation.
- Identified two mandatory Milestone 1.3 compatibility fixes: disable broken
  V1 hardware VBUS sensing and replace every embedded `%llu` path with bounded
  manual unsigned-64 decimal conversion.
- Added an explicit board-level USB VBUS mode. V1 selects `ASSUME_PRESENT`, so
  PA9 is neither initialized nor deinitialized and the OTG peripheral receives
  `vbus_sensing_enable = 0U`; a corrected V2 can select `SENSE_INPUT` without
  changing USB transport or application code.
- Added the allocation-free shared `uint64_decimal_format()` utility with an
  optional bounded zero-padded width and migrated status uptime, health fault
  timestamps, USB log timestamps/sequences, and canonical text logs away from
  long-long printf formatting.
- Recorded physically proven clocks, USB, sensor, microSD, and WS2812 behavior;
  confirmed unusable discrete LEDs; unresolved motor/receiver routing; and the
  phase that owns each future carryover in `docs/v1-bringup-carryover.md`.
- Programmed and read-back verified the Debug image from clean commit `5db525a`
  on a V1 STM32F405, then confirmed running `DISARMED`/`OK` state, `CAFE:4002`
  enumeration, JSON framing/correlation, host close/reopen, 168 MHz core state,
  TIM5 configuration/rate, and expected diagnostic task ratios.
- Added build-time firmware identity with separate semantic version and
  dirty-aware Git build ID, retained in the ELF, startup log, and status JSON.
- Added the reusable `./ofc` host application with independent build,
  programmer, USB, correlated-protocol, JSON-reporting, and non-arming smoke
  services. The CLI can build or flash either profile or a supplied ELF,
  inspect status, monitor JSON, and automate status/health smoke checks without
  tying those capabilities to terminal presentation.
- Added V1 boot-safe WS2812 handling after a physical reconnect exposed random
  green illumination from the previously undriven PA1 data line. Board startup
  now preloads PA1 low, emits one tester-proven all-zero GRB frame to clear
  retained LED state, and leaves the line low; it adds no status semantics or
  general RGB API.
- Added a pure outbound DShot frame encoder with separate throttle/stop and
  special-command entry points. It packs the 11-bit value, telemetry-request
  bit, and four-bit nibble-XOR checksum into one MSB-first `uint16_t` without
  hardware, scheduling, flight-model, allocation, or mutable-global coupling.
- Made throttle encoding accept only stop `0` or `48..2047`; reserved commands
  `1..47` require the explicit command API. Invalid inputs leave prior output
  unchanged, while command repetition and authorization remain future policy.
- Recorded the immutable V1 motor routes as `ESC_M1`/PC9/TIM8_CH4 through
  `ESC_M4`/PC6/TIM8_CH1, with AF3 selected for every pin and a 168 MHz TIM8
  input derived from the existing APB2 clock tree.
- Selected one TIM8-update timer DMA burst using DMA2 Stream 1/Channel 7 to
  update CCR1 through CCR4 together, rather than consuming four independent
  streams. No GPIO, TIM8, DMA, or motor output is initialized yet.
- Recorded the SpeedyBee BLS 60A 30x30 4-in-1 ESC with stock BLHeli_S J-H-40
  as the initial propeller-free validation target. It supports DShot300/600;
  initial output uses ordinary DShot300 with no telemetry request.
- Added a validated DShot300 timing profile for the V1 168 MHz TIM8 clock:
  560 ticks per bit, 210 ticks high for zero, and 420 ticks high for one.
- Added a pure 18-by-4 interleaved compare buffer: 16 MSB-first frame rows
  followed by two all-low rows, with lanes explicitly ordered CCR1 through
  CCR4 (`ESC_M4` through `ESC_M1`). No GPIO, TIM8, DMA, or motor output is
  initialized yet.
- Kept DShot600 as a later roadmap profile after DShot300 physical validation;
  it is intentionally rejected by the current API.
- Added a hardware-independent logical-to-physical motor permutation with an
  identity default, complete permutation validation, atomic replacement, and
  explicit requirements that the caller confirm both `DISARMED` lifecycle and
  accepted physical force-stop before reconfiguration.
- Added complete-command mapping through temporary local storage, including
  safe in-place use and canonical command revalidation. Runtime persistence,
  aircraft position names, expected CW/CCW direction, and ESC direction
  programming remain deliberately deferred.
- Added focused host suites for both configurable logical mapping and fixed V1
  resource facts, plus `docs/motor-output-mapping.md` covering evidence,
  ownership, DMA choice, resource conflicts, safety boundaries, and physical
  validation.

No motor output, receiver input, sensor access, persistent flight-data logging,
or flight-control behavior has been implemented. `motor_command_t`, the motor
mapping, and the generic output facade remain unconnected contracts with no
production backend, and the `ARMED` lifecycle state still has no actuator
effect.

## Known issues and limitations

- Milestone 0.2 had no board evidence, but the later manufacturing acceptance
  run has now proven SWD programming/reset and the HSE/PLL clock tree. The
  later flight-image Debug smoke independently passed; Release and stress
  coverage remain in the hardware checklist.
- The hardware repository was not available in the current workspace for this
  plan update. The prior reviewed hashes remain the design baseline; physical
  findings and tester commits provide the new evidence.
- The cooperative scheduler intentionally busy-polls when no task is ready. A future evidence-driven power/idle policy may sleep, but sleeping is not required for the current flight-control foundation.
- No GPIO or routed peripheral listed in the V1 hardware map is initialized by Milestone 0.4. TIM5 runs internally without timer pins or DMA.
- Physical verification of the flight image's 1 MHz TIM5 rate and continuous
  operation through a real or accelerated 71.58-minute hardware wrap remains
  pending.
- Correct 64-bit extension assumes global interrupts are not continuously disabled for a complete 71.58-minute TIM5 wrap period; ordinary bounded critical sections are many orders of magnitude shorter.
- Receiver UART, motor timing/register implementation, GPS PPS capture, IMU EXTI, and ADC sampling decisions remain deliberately deferred to their owning milestones.
- Host tests cover the portable overflow resolver; TIM5 register behavior and frequency still require the separate physical-board checks described above.
- Ready-batch fairness prevents selection starvation only when callbacks return. A non-returning callback blocks every task, and CPU overload still causes recorded missed releases.
- USB is now an explicit development/bench arm-request source, but it is unauthenticated and changes only lifecycle state. Actuator authorization and final output gating must be defined before later motor commands can affect hardware.
- State-machine mutation currently belongs to main context and is not an interrupt-safe concurrent API.
- Fault reporting and clearing also currently belong to main context and are not interrupt-safe concurrent APIs.
- The production catalogue contains current foundation failures plus ordinary logging-clock and USB-service faults; additional warning and non-critical IDs remain owned by their future subsystem milestones.
- When no USB host is configured, the transport accepts two complete output events and then reports busy; responses remain pending in the command processor and logs remain in the logging queue until reconnection or eventual queue overflow.
- Logging is main/cooperative-task-context only, not interrupt-safe, and bounded formatting still consumes execution time even though it never waits for output.
- The initial logger deliberately excludes floating-point formatting, synchronous immediate output, panic/crash transport, multiple backends, persistent storage, and high-rate flight-data recording.
- USB receive buffering is bounded and lossy under overload. Raw-ring overflow,
  full completed-line queues, and oversized lines are counted; affected input
  is discarded rather than partially executed.
- A health response may omit later fault details to stay within one 768-byte
  transport entry; total severity counts remain complete and `truncated`
  exposes the omission.
- Fault-registry exhaustion loses record detail and is reset-latched through
  terminal `FAULT`; `UNKNOWN` remains only a defensive inconsistent-snapshot
  outcome, not a normal public reporting path.
- The default USB VID/PID is explicitly development-only and must be replaced with assigned values before distributing hardware.
- The tester proved USB enumeration only after disabling the defective V1 VBUS
  sense path. The flight source now selects that behavior through a board mode
  and no longer uses long-long printf formatting, but flight-image USB,
  disconnect/reconnect, and throughput validation remain pending.
- The `0.001f` normalized stop threshold is a conservative software starting
  point. Its relationship to actual ESC startup behavior remains a propeller-free
  Phase 1 bench-validation item.
- Latest motor command storage, producer/consumer scheduling, timeout
  enforcement, lifecycle/health gating, and a real force-stop backend remain
  deliberately absent until their approved Phase 1 milestones.
- A backend descriptor is copied, but its non-null context object is not; that
  context must have static or otherwise sufficient lifetime. Accepted submission
  likewise promises only an internal copy, not immediate physical application.
- `motor_mapping_configure()` cannot independently inspect application state or
  hardware. Its future owner must pass `system_disarmed` only from actual
  `SYSTEM_STATE_DISARMED` and `outputs_stopped` only after force-stop has been
  accepted; the mapping module rejects false conditions but cannot detect a
  dishonest caller.
- Logical aircraft positions, mixer convention, expected CW/CCW directions,
  ESC-stored direction, and configuration persistence are not selected. No ESC
  direction command may be exposed as an ordinary runtime motor command.
- The selected V1 motor routes and DMA resources are documentation and
  host-tested data, not proof of PCB continuity, GPIO AF register behavior,
  TIM8/DMA execution, electrical waveform quality, ESC acceptance, physical
  motor order, or direction.

## Open questions

- Choose an open-source license before declaring the public source licensed for reuse.
- Decide later whether the exact GCC `15.3.1` reproducibility check should become a documented compatible version range; Milestone 0.2 deliberately matches the proven tester toolchain.
- Complete the documented flight-image SWD, timebase, USB, and lifecycle smoke
  tests on the now-available board and ST-Link.
- Measure WS2812 logic margin and interrupt-latency impact before any in-flight
  use. Discrete LED inoperability and active-low microSD detect are confirmed.

## Next step

Review Milestone 1.6, then begin Milestone 1.7 by implementing the final
lifecycle, health, command-freshness, and force-stop safety gate before any
timer output is activated. Staged single-channel TIM8/GPIO/DMA activation
follows in Milestone 1.8. DShot600 remains a roadmap extension after DShot300
works reliably. The outstanding foundation stress checks remain flight
prerequisites in `docs/hardware-validation-checklist.md`.

## Milestone 1.6 verification

- The normal and address/undefined-behavior sanitizer builds each run all 21
  host test executables successfully.
- DShot timing tests prove the exact 168 MHz DShot300 profile: 560 timer ticks
  per bit, a 210-tick zero high, and a 420-tick one high.
- Tests cover all 16 bit positions in all four timer lanes, MSB-first output,
  the exact documented 25%/50% mixed frame, two trailing all-low slots, source
  and destination overlap, invalid pointers, unsupported rates/clocks,
  corrupted profiles, and failure preservation.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 53,252 bytes of Flash and reserves
  14,744 bytes of RAM; Release uses 35,408 bytes of Flash and reserves 14,744
  bytes of RAM.
- The implementation is pure bounded C with no allocation, mutable global,
  normalized-throttle conversion, state/fault access, HAL/register access,
  interrupts, GPIO, TIM8 activation, DMA activation, or physical motor output.
- The SpeedyBee BLS 60A/J-H-40 is recorded as the initial propeller-free test
  ESC. Its supported DShot600 mode remains deliberately unavailable until a
  later separately reviewed and physically validated roadmap extension.
- No physical verification is claimed. Timer preload/DMA pipeline behavior,
  waveform timing and voltage, ESC recognition, output ordering, direction,
  and repeated transfers remain staged hardware tests.

## Milestone 1.5 verification

- The normal and address/undefined-behavior sanitizer builds each run all 20
  host test executables successfully.
- Board-map tests prove physical output indices zero through three retain the
  exact `ESC_M1`/PC9/CH4 through `ESC_M4`/PC6/CH1 order, AF3, TIM8, 168 MHz
  timer clock, DMA2 Stream 1/Channel 7, and four-register CCR1-first burst.
- Motor-mapping tests exhaustively classify all 256 in-range assignments
  (exactly 24 permutations), and prove identity initialization, every
  unsafe-state combination, out-of-range rejection, atomic preservation,
  complete command reordering, in-place operation, and corrupted
  mapping/command rejection.
- The fixed route agrees with the retained manufacturing-test board definition
  and STM32F405 datasheet AF table. RM0090 confirms the selected TIM8 update DMA
  request and timer DMA-burst mechanism; no individual-channel stream is
  reserved.
- Resource review found no conflict with current TIM5, USB, or WS2812 use. The
  single-stream choice preserves DMA2 streams needed by future SPI1 and ADC
  implementations, whose final allocations remain their own milestones.
- Debug and Release firmware configurations build with warnings treated as
  errors. The unconnected mapping data and logic are eligible for section
  garbage collection and produce no runtime hardware behavior.
- No physical verification is claimed. Pin muxing, output enable, burst
  pipeline, waveform timing, electrical levels, ESC recognition, output order,
  and direction remain staged propeller-free checks.

## Milestone 1.4 verification

- The normal and address/undefined-behavior sanitizer builds each run all 18
  host test executables successfully.
- A dedicated host suite exhaustively encodes all 2,048 values with telemetry
  both clear and set, covering the full 4,096-frame ordinary DShot space.
- Tests compare against an independent iterative-nibble checksum calculation,
  recover value and telemetry fields, and check documented value 1046 without
  telemetry as exact frame `0x82C6`.
- Tests prove stop and throttle cannot enter the reserved command range,
  commands cannot enter stop/throttle ranges, out-of-range values are rejected,
  null destinations are rejected, and failures preserve existing output.
- The module is pure C with fixed execution and no HAL, STM32, timer, DMA, GPIO,
  float, `motor_command_t`, scheduler, allocation, or mutable-global dependency.
- Correct host frames are not physical DShot evidence; timing, bit-to-duty
  representation, DMA ordering, routing, voltage, and ESC acceptance remain in
  later milestones.

## Milestone 1.3 software verification

- The Python host package runs 16 focused tests covering command parsing,
  partial USB framing, VID/PID discovery, log/response demultiplexing, request
  correlation, firmware presets and identity extraction, probe selection,
  non-arming smoke behavior, and JSON reporting.
- A Debug build generated `firmware_version=0.1.0`, the expected
  `5db525a-dirty` worktree build ID, and `v0.1.0+git.5db525a.dirty`; all three
  strings are retained in the ELF. A clean post-commit build must be physically
  matched to its running status response before the checklist closes.
- The normal and address/undefined-behavior sanitizer host builds each run 17
  test executables; all tests pass.
- Dedicated tests prove both USB VBUS modes, the V1 assume-present selection,
  decimal conversion from zero through `UINT64_MAX`, zero padding, invalid
  arguments, and exact-capacity rejection.
- Existing logging, status, health, and command tests pass after migration;
  focused assertions cover exact maximum 64-bit uptime, timestamps, and
  sequences.
- No project-owned production source contains a long-long printf conversion or
  `unsigned long long` formatting cast. The pinned newlib-nano is no longer
  responsible for serializing 64-bit diagnostics.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. With embedded identity, status fields, and the
  boot-only RGB safe-off frame, Debug uses 53,252 bytes of Flash and 14,744
  bytes of RAM; Release uses 35,408 bytes of Flash and 14,744 bytes of RAM.
- The Release ELF has no unresolved symbols, clangd reports zero errors for the
  new common formatter and changed board USB port, and Git whitespace
  validation passes.
- Physical Debug-image USB enumeration and ordinary target output passed after
  this software work; Release, forced numeric boundaries, and stress cases
  remain explicitly pending rather than inferred from host/link results.

## Milestone 1.2 verification

- The host-development build runs fifteen test executables; all tests pass.
- Motor-output tests cover missing callbacks, backend initialization failure,
  initial force-stop failure, fail-closed unknown results, successful readiness,
  copied descriptor lifetime, invalid/uninitialized calls, caller-independent
  canonical storage, manually corrupted commands, backend busy/error mapping,
  and force-stop's lack of a busy outcome.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. The unconnected facade is removed by section
  garbage collection, so Debug remains 52,080 bytes of Flash and 14,736 bytes
  of RAM, while Release remains 34,364 bytes of Flash and 14,736 bytes of RAM.
- The facade contains no STM32/HAL dependency, production backend, peripheral
  behavior, global mutable state, runtime allocation, DShot representation, USB
  schema, state/health lookup, or timeout enforcement.
- Address/undefined-behavior sanitizer execution of all fifteen host suites
  passes, clangd reports zero errors for both actuator production modules, Git
  whitespace validation passes, and the Release ELF has no unresolved symbols.
- Physical verification is not applicable to this interface-only milestone;
  each future real backend must separately prove its copy, pending-demand
  override, timing, and electrical stop behavior.

## Milestone 1.1 verification

- The host-development build runs fourteen test executables; all tests pass.
- Motor command tests cover invalid initialization, all normalized boundaries,
  atomic four-value creation, inclusive stop-threshold canonicalization,
  all-zero valid stop, NaN/infinity/out-of-range rejection without replacement,
  invalid arguments, invalidation, exact timeout boundaries, future timestamps,
  and safe freshness arithmetic near `UINT64_MAX`.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. The currently unreferenced model is removed by
  section garbage collection, so Debug remains 52,080 bytes of Flash and 14,736
  bytes of RAM, while Release remains 34,364 bytes of Flash and 14,736 bytes of
  RAM.
- The module contains no STM32/HAL dependency, peripheral behavior, global
  mutable state, runtime allocation, DShot representation, USB schema, or
  floating-point formatting.
- Address/undefined-behavior sanitizer execution of all fourteen host suites
  passes, clangd reports zero errors for the production module, Git whitespace
  validation passes, and the Release ELF has no unresolved symbols.
- Physical verification is not applicable to this value-only milestone; the
  stop threshold must still be validated with the ESC during the later
  propeller-free bench milestone.

## Milestone 0.13 verification

- The host-development build runs thirteen test executables; all tests pass.
- The new foundation integration suite covers successful and degraded startup,
  terminal startup failure, ordinary and critical faults while armed, recovery,
  and fault-registry exhaustion across the state, fault, and health modules.
- USB protocol and command tests cover required IDs in arbitrary member order,
  zero and `UINT32_MAX`, missing/duplicate/noncanonical/overflowing IDs, exact
  correlation on every response, and `null` on malformed envelopes.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 52,080 bytes of Flash and reserves
  14,736 bytes of RAM; Release uses 34,364 bytes of Flash and reserves 14,736
  bytes of RAM.
- Address/undefined-behavior sanitizer execution of all thirteen host suites
  passes. clangd reports zero errors for all changed production C units, Git
  whitespace validation passes, and the Release ELF has no unresolved symbols.
- The reviewed code retains fixed-capacity storage and contains no runtime
  allocation. USB callbacks remain limited to byte copying and flag updates;
  parsing, formatting, state transitions, and transport queue progression stay
  in cooperative main context.
- All physical USB, overload, execution-time, stack, timebase, and board checks
  are explicitly pending in `docs/hardware-validation-checklist.md` until the
  required boards and debug hardware arrive.

## Milestone 0.12 verification

- The host-development build runs twelve test executables; all tests pass.
- Health tests cover all five overall states, severity precedence/counts,
  clearing recovery, lifecycle-critical override, dropped-record uncertainty,
  invalid inputs, stable names, exact empty output, complete record metadata,
  fixed-capacity truncation, and USB command integration.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1.
- Debug uses 51,560 bytes of Flash and reserves 14,736 bytes of RAM.
- Release uses 34,148 bytes of Flash and reserves 14,736 bytes of RAM.
- Address/undefined-behavior sanitizer execution of all twelve host suites
  passes, and clangd checks of all changed production C units report zero
  errors.
- Health evaluation scans at most 16 slots; response serialization uses only
  fixed stack buffers and the existing command response storage.
- Arm GCC stack-usage output reports static local frames for the health command
  path; the nested service, processor, serializer, and formatting frames remain
  within the firmware's reserved 2 KiB minimum stack before library internals.
- Physical structured-health output remains unverified without a connected
  Flight Computer V1.

## Milestone 0.11 verification

- The host-development build runs ten test executables; all tests pass.
- New tests cover fragmented LF/CRLF input, empty and multiple lines, exact and
  oversized bounds, recovery, all four commands, strict malformed-object
  rejection, exact response bytes, accepted/rejected state transitions,
  response backpressure/order, and JSON log escaping.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1 and retain the full STM32 USB integration.
- Debug uses 48,864 bytes of Flash and reserves 14,736 bytes of RAM.
- Release uses 32,600 bytes of Flash and reserves 14,736 bytes of RAM.
- Address/undefined-behavior sanitizer execution of all ten host test suites
  passes, and clangd checks of all changed production C integration units report
  zero errors.
- All receive, parse, dispatch, serialization, and queue storage is fixed-size;
  there is no runtime dynamic allocation or parsing/state mutation in ISR
  context.
- Physical USB receive, interactive commands, overflow recovery, enumeration,
  and disconnect/reconnect remain unverified without a connected board.

## Milestone 0.10 verification

- The host-development build runs seven test executables; all tests pass.
- USB logging adapter tests verify exact canonical output bytes, transport-owned copying, accepted removal, busy retention/retry, and error removal/counting.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1 and link the STM32CubeF4 USB Device CDC stack.
- Debug uses 41,752 bytes of Flash and reserves 11,360 bytes of RAM.
- Release uses 27,968 bytes of Flash and reserves 11,352 bytes of RAM.
- Address/undefined-behavior sanitizer execution of all seven host test suites passes.
- clangd checks of the application integration, backend, transport, descriptors, and V1 USB port report zero errors.
- The USB implementation contains no runtime dynamic allocation; STM USB class allocation is redirected to fixed static storage.
- Application sources contain no STM32 HAL calls, and USB formatting/draining remains outside interrupt context.
- Physical-board USB behavior remains unverified because no connected board session was available.

## Milestone 0.9 verification

- The host-development build runs timebase, task, scheduler, system-state, fault, and logging test executables; all tests pass.
- Logging tests cover default configuration and names, global/per-module filtering, `OFF`, filtered argument suppression, timestamp validity, sequences, bounded formatting, truncation, canonical lines, FIFO ordering and wrap, queue overflow and sequence gaps, backend busy/retry/accept/error behavior, invalid operations, and saturating counters.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 15,196 bytes of Flash and reserves 8,032 bytes of RAM.
- Release uses 10,916 bytes of Flash and reserves 8,032 bytes of RAM.
- Address/undefined-behavior sanitizer execution of the host logging tests passes.
- Logging sources contain no STM32/HAL dependency, peripheral access, USB behavior, runtime allocation, or synchronous output path.
- Physical-board behavior remains unverified because no board/ST-Link session was available; this milestone adds no new peripheral configuration.

## Milestone 0.8 verification

- The host-development build runs timebase, task, scheduler, system-state, and fault test executables; all tests pass.
- Fault tests cover catalogue validation, warning/fault/critical behavior, critical transitions from all six lifecycle states, startup arm prevention, record preservation, early/valid timestamps, repeated reports, context updates, clearing and latching, capacity exhaustion, critical safety under overflow, slot reuse, invalid operations, and saturating counters.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 10,640 bytes of Flash and reserves 3,984 bytes of RAM.
- Release uses 7,064 bytes of Flash and reserves 3,984 bytes of RAM.
- Address/undefined-behavior sanitizer execution of the host fault tests passes.
- The fault and catalogue modules contain no STM32/HAL dependency, peripheral access, logging output, or runtime allocation.
- Physical-board behavior remains unverified because no board/ST-Link session was available; this milestone adds no new peripheral configuration.

## Milestone 0.7 verification

- The host-development build runs timebase, task, scheduler, and system-state test executables; all tests pass.
- State tests cover every one of the 36 state/event combinations, legal and illegal transitions, explicit-only arming, failsafe disarm recovery, terminal `FAULT`, invalid arguments, initialization, previous-state preservation, and saturating counters.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 8,816 bytes of Flash and reserves 3,312 bytes of RAM.
- Release uses 5,808 bytes of Flash and reserves 3,312 bytes of RAM.
- The state module contains no STM32/HAL dependency, peripheral behavior, runtime allocation, or concurrent interrupt mutation.
- Physical-board behavior remains unverified because no board/ST-Link session was available; this milestone adds no new peripheral configuration.

## Milestone 0.6 verification

- The host-development build runs timebase, task, and scheduler test executables; all tests pass.
- Scheduler tests use a deterministic fake clock and cover invalid state, immediate first release, idle timing, priority/release/registration tie-breaking, disabled tasks, execution measurements, phase-preserving advancement, skipped releases, overrun detection, and ready-batch starvation protection.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 8,136 bytes of Flash and reserves 3,288 bytes of RAM.
- Release uses 5,596 bytes of Flash and reserves 3,288 bytes of RAM.
- The Release ELF has no unresolved symbols and retains `scheduler_initialize`, `scheduler_run_once`, Task registry functions, the application registry/scheduler, and all three diagnostic counters for inspection.
- Address/undefined-behavior sanitizer execution of the host scheduler tests passes.
- Scheduler and Task sources contain no STM32/HAL dependency or runtime allocation, clangd checks pass, and Git whitespace validation passes.

## Milestone 0.5 verification

- The host-development build runs both the timebase and task test executables; all tests pass.
- Task tests cover empty initialization, valid metadata copying, zeroed runtime state, invalid arguments, bounded names, zero periods, null callbacks, duplicate names, priority preservation, deterministic registration order, bounds-checked lookup, and all 16 capacity slots.
- Debug and Release firmware configurations compile `app/task.c` with warnings treated as errors using Arm GCC 15.3.1.
- The linker correctly removes the currently unreferenced task object from the final image because Milestone 0.5 deliberately does not instantiate the scheduler.
- Debug and Release image sizes therefore remain unchanged from Milestone 0.4.
- Application and task sources contain no STM32/HAL dependencies or runtime allocation.

## Milestone 0.4 verification

- The host-development build runs one native timebase test executable; all tests pass.
- The tests execute the same snapshot resolver linked into the firmware and cover normal reads, a pending update before interrupt service, an overflow interrupt racing a read, and monotonic values across `0xffffffff` to zero.
- Debug and Release firmware configurations build with warnings treated as errors using Arm GCC 15.3.1.
- Debug uses 5,956 bytes of Flash and reserves 2,088 bytes of RAM.
- Release uses 3,940 bytes of Flash and reserves 2,088 bytes of RAM.
- The Release ELF is a statically linked, 32-bit little-endian ARM EABI5 hard-float executable with no unresolved symbols.
- The TIM5 vector slot resolves to the implemented Thumb `TIM5_IRQHandler` rather than the startup default handler.
- `time_us`, `mcu_timebase_initialize`, `mcu_timebase_us`, `mcu_timebase_handle_overflow_interrupt`, `timebase_snapshot_resolve`, `TIM5_IRQHandler`, and `firmware_uptime_us` remain inspectable symbols.
- Application source contains no STM32/HAL register access, and Git whitespace validation passes.

## Milestone 0.3 verification

- Clean Debug and Release configurations build with CMake 4.4.2, Ninja 1.13.2, and Arm GCC 15.3.1.
- Debug uses 5,064 bytes of Flash and reserves 2,072 bytes of RAM.
- Release uses 3,548 bytes of Flash and reserves 2,072 bytes of RAM.
- The Release ELF is a statically linked, 32-bit little-endian ARM EABI5 hard-float executable.
- The vector table is linked at `0x08000000`, the Thumb reset entry is `0x08000335`, and the stack top remains `0x20020000`.
- `main`, `board_initialize`, `board_halt`, `mcu_initialize`, `mcu_halt`, `system_clock_configure`, `firmware_boot_status`, and `firmware_main_loop_iterations` are retained as inspectable symbols.
- The ELF has no unresolved symbols, and ELF, HEX, BIN, map, and compile-command artifacts are present in both profiles.
- A source-boundary check confirms `firmware/app` contains no STM32/HAL includes, calls, registers, or CMSIS intrinsics; those appear only below the hardware boundary.
- The authoritative KiCad project, schematic, and PCB hashes still match the recorded hardware snapshot.
- Git whitespace validation passes.
- The host-development preset still configures and builds cleanly; CTest correctly reports no tests for this milestone.
