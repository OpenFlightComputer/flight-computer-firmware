# Development status

## Current phase

Phase 4 — stabilization and first hover.

## Current milestone

Milestone 4.8 — stabilized receiver mixing and fail-closed IMU authority —
implemented in software and awaiting owner review. It is connected to motor
output but has not yet been physically validated.

## Last completed milestone

Phase 3 open-loop receiver-to-motor integration. The owner physically confirmed
that the controller can arm and drive the secured, propeller-free motors through
the receiver, mixer, authority gate, and DShot output path.

## Current implementation status

- Added a flat hardware-independent IMU processing pipeline after the existing
  sample publication and startup gyro calibration. It converts mapped counts
  to g/degrees per second, subtracts the frozen bias, filters all three gyro
  axes, derives an accelerometer roll/pitch reference, and publishes the latest
  complementary-filter estimate.
- Added independently replaceable `gyro_filter` and `attitude_estimator`
  modules. The initial selections are a first-order low-pass filter at 80 Hz
  and a complementary estimator with a 0.5 s accelerometer correction time
  constant.
- Derives every filter/estimator step from consecutive acquisition timestamps.
  Duplicate samples do no work; stale data, time/sequence rollback, and gaps
  over 10 ms invalidate continuity instead of integrating missing time.
- Moved calibration sample consumption fully into the temporary high-priority
  1 kHz startup task. It disables itself after entering `DISARMED`; the
  permanent IMU task now performs only acquisition, processing, and runtime
  health/diagnostics.
- Extended the unified configuration to schema 3. Filter/estimator selections
  and parameters persist, migrate from schemas 1 and 2, and are reapplied
  immediately by a successful disarmed write. Reapplication resets processing
  history; startup-calibration policy still takes effect on the next boot.
- Extended the unified configuration to schema 4 with separate roll, pitch,
  yaw, and throttle curves; centered-axis deadbands; maximum roll/pitch angles;
  maximum axis rates; throttle zero deadband; and maximum throttle.
- Added a bounded control-point curve representation with two through eight
  monotonic points. Linear interpolation is implemented now, while a prepared
  four-coefficient segment representation keeps quadratic and monotone-cubic
  interpolation replaceable without changing the flight-loop interface.
- Prepared curve coefficients, deadband reciprocals, the quad-X mixer matrix,
  receiver normalization reciprocals, and the gyro-filter time constant
  outside their high-frequency paths.
- Kept receiver acquisition and normalization separate from control shaping.
  The 1 kHz flight-control task applies the prepared shaping before the mixer;
  exact zero throttle exits before axis work. USB motor tests and the physical
  Stage 1 failsafe command deliberately bypass pilot input shaping.
- Added desired roll/pitch angle and yaw-rate setpoints for the later closed-loop
  controllers. Current motor output remains open-loop; roll/pitch rate limits
  are configuration prepared for the self-leveling outer loop and do not yet
  provide stabilization.
- Expanded the request-driven whole-document JSON protocol and persistent
  payload for schema 4. Schema 1-3 and legacy motor records migrate by retaining
  their known fields and filling new control settings from compiled defaults.
- Added reusable single-axis PID and three-axis rate-controller modules. The
  derivative acts on measured gyro rate, integration is conditionally blocked
  at output saturation and independently clamped, and controller output is
  bounded per axis.
- Derived PID `dt` from consecutive acquisition timestamps. Duplicate,
  reversed, invalid, and over-gap IMU samples cannot update a correction;
  disabling the controller clears integrals and measurement history.
- Extended the unified configuration and USB protocol to schema 5 with
  independent roll/pitch/yaw PID gains and limits plus a maximum sample gap.
  The persistent payload is 480 bytes while the fixed record remains 536
  bytes; schemas 1-4 migrate with new PID fields supplied by defaults.
- Kept Milestone 4.6 isolated from production motor authority. It adds no task
  and performs no runtime shadow calculation; the existing open-loop path is
  unchanged until the outer loop and stabilized integration milestones.
- Added separate hardware-independent roll, pitch, and yaw attitude-control
  modules. Roll and pitch convert angle error to desired rate with a
  configurable default gain of `4.0 s^-1`; yaw directly preserves the pilot's
  requested rate until an absolute-heading sensor exists. Flight control calls
  all three directly, without function-pointer dispatch or a coordinator.
- Integrated the attitude and rate controllers as shadow calculations in the
  existing 1 kHz flight-control task. PID state resets at exact zero throttle,
  failsafe, lost receiver authority, or unavailable IMU data. Open-loop motor
  commands remain byte-for-byte independent of the shadow result.
- Extended configuration, persistence, and USB transport to schema 6. The new
  488-byte payload migrates schemas 1-5 and stores the separate roll and pitch
  angle gains; no redundant type or axis-mode fields are persisted.
- Extended the existing request-driven `imu` response and host visualization
  with filtered gyro, estimated roll/pitch, and bounded processing counters.
  No extra sensor read, snapshot producer, or scheduled diagnostics task was
  introduced.
- Increased both BMI270 output data rates from the Milestone 4.1 boot setting
  of 100 Hz to 1.6 kHz, then added a high-priority 1 kHz IMU service. Each run
  performs one bounded six-axis register read; no interrupt or dynamic
  allocation was introduced.
- Added a hardware-independent signed-permutation mapping and a complete owned
  sample snapshot containing mapped raw acceleration and gyro values, a
  microsecond acquisition-completion timestamp, a saturating 64-bit sequence,
  and validity. A failed read preserves the last good snapshot.
- Defined the provisional V1 installation convention as PCB top forward and
  component side up, mapping body forward/right/down to sensor +Y/+X/-Z. The
  authoritative PCB confirms the unrotated package but has no explicit front
  marker, so every sign remains subject to Milestone 4.3 physical validation.
- Added freshness classification: at most 2 ms is fresh, over 2 ms through
  10 ms is stale, and over 10 ms or clock rollback is lost. A recoverable IMU
  communication fault and transition-only logs cover persistent runtime
  failures; a new valid sample clears the fault. The separate initialization
  fault still prevents scheduling when boot validation fails.
- Reduced USB service from 1 kHz to 500 Hz and the placeholder diagnostics from
  1 kHz/100 Hz/10 Hz to 100 Hz/10 Hz/1 Hz. Motor output, IMU acquisition,
  receiver service, and the current open-loop flight-control step remain at
  1 kHz because each owns a distinct deadline.
- Added a 10 Hz conservative load diagnostic that sums the recorded maximum
  execution times of every enabled 1 kHz task, exposes the budget and permille
  utilization to the debugger, and logs once at or above 70% of the 1 ms
  period. Exact on-board execution times remain a required physical check.
- Split the former mixed `application_tasks.c` implementation into one source
  file per scheduled task. Each module now owns its callback, task definition,
  and private transition state; `application_tasks.c` only preserves conditional
  registration and deterministic ordering.
- Added native mapping/freshness and service/publication tests.
- Added strict `imu` USB request/response handling. It copies the existing
  coherent body-axis sample, freshness, acquisition counters, IMU task timing,
  and combined 1 kHz worst-case budget only when requested. It neither reads
  SPI nor schedules another task, and requests fail closed while `ARMED` or
  `FAILSAFE`.
- Added `./ofc device imu` and `./ofc device imu --watch`. The host converts
  the configured BMI270 raw counts to g and degrees per second and renders
  independent acceleration and gyro bars using the tester-proven scales.
  Watch polling is host-driven and bounded to at most 10 requests per second.
- Added a bounded, hardware-independent stationary gyro calibration, now fed
  by the temporary 1 kHz startup task. It waits 100 ms, then requires both
  500 ms and 500
  distinct fresh samples; excessive instantaneous rate or end-of-window
  variance restarts collection.
- Accumulated signed 64-bit sums and squared sums produce one rounded bias per
  body axis. Accepted biases remain RAM-only and immutable until reboot; the
  result is diagnostic-only and does not yet influence motor output.
- Startup now remains in `INITIALIZING` until calibration succeeds. A separate
  startup task owns the final transition to `DISARMED`, leaving a clear place
  for additional future startup requirements.
- Task callbacks can now explicitly return `TASK_CALLBACK_DISABLE`; the
  scheduler records the final invocation and then excludes the task from later
  ready batches. The startup task uses this after its successful transition to
  `DISARMED`, avoiding a permanent 1 kHz no-op.
- Extended the unified configuration to schema 2 with gyro settling, sampling,
  maximum-rate, and maximum-standard-deviation settings. Persistent schema-1
  documents migrate with their prior fields preserved and new values filled
  from compiled defaults.
- Extended the tester-proven PA1 WS2812 implementation to full GRB output:
  yellow during initialization, green in `DISARMED`, and off before arm
  preparation. Noncritical updates use a 10 Hz background task so disarming is
  not delayed by the roughly 2 ms reset-bounded LED transaction.
- Extended `imu` diagnostics and the host view with calibration state,
  progress, samples, restarts, raw bias, and bias-corrected gyro values.
- All 49 native tests and all 67 Python host-tool tests pass, including the
  address/undefined-behavior sanitizer build. Debug and Release firmware build
  with warnings as errors. Debug uses 172,684 bytes of Flash and 48,296 bytes
  of RAM; Release uses 119,740 bytes of Flash and 48,280 bytes of RAM. The RAM
  increase is primarily the bounded 4,096-byte configuration lines and USB
  queues required by maximum-size curve documents.
- Replaced the open-loop receiver mixer input with the complete stabilized
  chain: shaped angle/rate setpoints, roll/pitch/yaw attitude stages, the
  three-axis rate PID, and a pure-sign props-in/props-out quad-X matrix.
- Added proportional correction-span scaling and a common collective shift so
  saturated outputs remain within `0.0..1.0` without independently clipping
  away their relative corrections. Exact zero throttle still exits first,
  resets controller history, and submits four exact zeros.
- Added the IMU authority gate. Fresh new estimates may update motor output;
  duplicate or 2-10 ms stale data submits nothing and lets the motor layer hold
  its last complete accepted command; lost, future, invalid, or incoherent IMU
  data enters the central failsafe. The first PID sample after a reset seeds
  continuity and deliberately submits one exact-zero command.
- Routed the receiver Stage 1 fallback through the same stabilization path.
  Its configured normalized axes bypass pilot curves but now describe level
  angle/rate requests rather than open-loop motor corrections.
- Advanced the whole-document configuration to schema 7 and removed the
  obsolete open-loop mixer factors. The persistent binary format remains 488
  bytes with those old slots reserved, allowing schema-6 records to migrate
  while PID output limits become the single correction-authority bound.

## Phase 4 assumptions and safety boundary

- The provisional V1 body mapping assumes the PCB top is aircraft forward and
  the component side is up. This is not yet physical evidence.
- Polling the latest 1.6 kHz sensor registers from a 1 kHz task is intentional;
  the firmware does not claim to consume every sensor update.
- Software builds cannot prove the 1 kHz deadlines, SPI recovery behavior, or
  physical axis signs. All remain explicitly pending on-board measurement.
- IMU data is not yet an input to motor control, so the new shaping changes
  stick response but does not pretend to provide stabilization.
- The displayed engineering units use the configured ±2 g and ±2000
  degree-per-second ranges. They are diagnostic conversions, not yet calibrated
  control values.
- Physical axis signs, live sample rate, freshness, and scheduler timings still
  require validation on the flashed flight image.
- A perfectly constant slow rotation is mathematically indistinguishable from
  a stationary sensor bias. The instantaneous-rate and variance gates reject
  ordinary movement, but the operator must still keep the vehicle motionless.
- Configuration changes to startup-calibration settings are persisted while
  disarmed and take effect on the next reboot; they do not invalidate the
  already accepted RAM-only bias during the current boot.
- The verified WS2812 pulse timings are reused from the manufacturing tester,
  but the complete lifecycle color sequence still requires flight-image
  validation.
- The accelerometer correction is intentionally not gated by acceleration
  magnitude in this initial forgiving angle-mode design. More advanced
  free-fall/aggressive-mode behavior remains explicitly deferred.
- The estimate and controllers now have receiver-path motor authority. They
  remain software-verified only and require the full Milestone 4.9
  propeller-free validation before any hover attempt.
- The initial throttle curve is intentionally linear after a 2% zero deadband.
  A lift-off plateau must be based on physical vehicle measurements rather than
  guessed before the first controlled tests.
- The conservative 30-degree angle, 180-degree-per-second roll/pitch, and
  150-degree-per-second yaw defaults are starting values, not flight-proven
  tuning.
- The `4.0 s^-1` roll/pitch angle gain and all PID gains remain conservative
  software defaults. Controller output now affects receiver-owned motors and
  must not be flown before the Milestone 4.9 propeller-free sign, timing,
  correction, saturation, and failure checks pass.
- Curve configuration affects live and held receiver input only. It cannot
  reshape USB bench commands or the explicitly configured receiver failsafe.
- A brief stale IMU interval deliberately retains the last complete motor
  command through the existing 100 ms motor lease. IMU loss after 10 ms enters
  the central failsafe; this threshold and the retained-command behavior need
  physical timing validation before flight.
- An IMU-triggered failsafe is intentionally fail-closed and requires an
  explicit disarm/recovery path before rearming. It does not silently resume
  motor authority when samples return.

## Next proposed milestone

Milestone 4.9 — add the request-driven control diagnostics needed for physical
IMU, setpoint, PID, mixer-saturation, motor-command, and failure validation,
then perform the complete propeller-free validation sequence.

## Historical milestone record

- Added one absolute `NORMAL`/`REVERSED` setting per logical motor with
  compiled defaults of all `NORMAL`, validation, and no toggle operation.
- Added disarmed-only USB and CLI show/set/reset paths. A successful set is
  persisted before replacing the active configuration, while an erase restores
  compiled defaults.
- Reserved STM32F405 sector 11 at `0x080E0000` for append-only configuration
  records with schema and format versions, sequence numbers, CRC32, and a
  commit word written last. Normal linked images exclude the sector; a mass
  erase clears it, and incompatible/corrupt storage fails startup closed.
- Added synchronized DShot direction submission: command 20 for `NORMAL`, 21
  for `REVERSED`, and the request bit set on all four physical outputs.
- Reasserted all four directions with ten frames after a configuration change
  and before every arm. The lifecycle stays `DISARMED` with an internal pending
  source until completion; disarm, receiver switch-low, or unusable receiver
  input can cancel a pending receiver arm.
- Updated `./ofc device arm` to wait for pending direction preparation. The
  initial per-field direction CLI from this milestone was later removed in
  favor of the unified whole-document configuration commands.
- All 35 native tests and 53 host-tool tests pass. Debug and Release firmware
  build with Arm GCC 15.3.1 and warnings as errors; Debug uses 94,756 bytes of
  application Flash and 16,720 bytes of RAM, while Release uses 64,320 bytes
  and 16,720 bytes. The separate 128 KiB configuration region has no linked
  image content. Flashing and physical persistence/direction checks remain
  pending owner review.

- Added a receiver arming interlock called by the existing 1 kHz receiver task
  after normalization, freshness, and failsafe evaluation; no new scheduler
  task or lifecycle state was added.
- Required valid, fresh, `LIVE` input, a previously observed low arm switch, a
  subsequent low-to-high edge, and normalized throttle at or below `0.001`
  before requesting `motor_control_arm(RECEIVER)`.
- Cleared arm qualification across stale/unavailable input and while another
  source owns motor control. Startup-high, reconnect-high, and lowering
  throttle while the switch remains high therefore cannot arm unexpectedly.
- Routed a fresh low switch through the common disarm method only when the
  receiver owns control. USB test ownership is isolated, and successful
  disarm leaves `NONE` available for either source's next explicit arm.
- Added bounded result/counter diagnostics and transition-only action logging.
  Receiver motor-command submission remains deliberately absent until the
  later mixer and producer milestones.
- Added host coverage for configuration, startup-high rejection, safe
  low-to-high arming, repeated frames, throttle rejection and retoggle,
  freshness/failsafe reset, receiver disarming, USB isolation, and common
  lifecycle rejection.
- All 33 native tests and 48 host-tool tests pass. Debug and Release firmware
  build with Arm GCC 15.3.1 and warnings as errors; Release uses 59,072 bytes
  of Flash and 16,656 bytes of RAM. Physical switch validation and flashing
  remain pending owner review.

- Added a single `NONE`/`USB_TEST`/`RECEIVER` authority latch inside the
  existing motor-control safety owner; this is orthogonal metadata, not a new
  lifecycle state machine.
- Centralized production arming in `motor_control_arm()`: source validation,
  lifecycle, health, and stop-frame preparation must all pass before the
  source is latched.
- Tagged every USB motor-test submission as `USB_TEST`. Commands from a source
  that did not arm are rejected without invalidating or replacing the active
  owner's retained command.
- Added source-independent disarm and fail-closed authority clearing on
  disarm, failsafe, non-armed synchronization, and successful or failed
  emergency force-stop.
- Added `control_source` to the USB `status` response and host coverage for
  exclusive ownership, rejection, handoff after disarm, admission failures,
  and readable source names. Milestone 3.2 now connects receiver arming;
  receiver motor submission remains deliberately disconnected until the later
  mixer and producer milestones.

- Added a read-only `receiver` USB command which copies the receiver service's
  existing raw and normalized snapshots only when requested. It reports packet
  age/freshness, failsafe state, link statistics, and UART/parser/DMA counters
  without reading DMA or adding work to the 1 kHz receiver task.
- Added `./ofc device receiver` and `./ofc device receiver --watch`, including
  tester-style raw channel positions and process-local minima/maxima alongside
  the flight firmware's normalized roll, pitch, yaw, throttle, and arm values.
- Added bounded response, command-dispatch, protocol-validation, and Rich-view
  host tests. The flashed flight image then displayed live RP1 raw channels,
  normalized controls, freshness, failsafe state, link data, and diagnostics in
  the on-demand host view, completing Milestone 2.6 and Phase 2.

- Added a hardware-independent, configurable receiver-loss policy with exact
  fresh, stale-hold, loss-hold, Stage 1 fallback, and latched Stage 2 timing.
- Set the initial Stage 1 request to neutral roll/pitch/yaw and 5% throttle,
  with the explicit roadmap requirement to replace it with measured,
  stabilized vehicle recovery later.
- Required fresh, arm-low, low-throttle input for a continuous recovery window
  before Stage 2 can be explicitly released; clock rollback fails closed.
- Integrated transition-only logging, debugger-visible policy diagnostics,
  and recoverable receiver-connection fault reporting into the 1 kHz receiver
  task while preserving the Phase 2 prohibition on motor commands.
- Added host coverage for configuration validation, exact timing boundaries,
  fallback values, automatic short-loss recovery, Stage 2 recovery/reset,
  unavailable input, and clock rollback.
- Ported the tester-proven UART4 circular-DMA accounting correction: absolute
  producer/consumer counts, a transfer-complete wrap epoch, volatile DMA-owned
  memory, explicit barriers, and detectable overruns with dropped-byte totals.
- Confirmed the 1 kHz high-priority receiver task is independent of the
  background USB/logging task. The corrected tester received 135,014 bytes
  without DMA drops, overruns, UART faults, CRC errors, or framing errors.

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
  560 ticks per bit, currently 210 ticks high for zero and 420 ticks high for
  one for the pulse-width isolation experiment.
- Added a pure 18-by-4 interleaved compare buffer: 16 MSB-first frame rows
  followed by two all-low rows, with lanes explicitly ordered CCR1 through
  CCR4 (`ESC_M4` through `ESC_M1`). No GPIO, TIM8, DMA, or motor output is
  initialized yet.
- Kept DShot600 as a later roadmap profile after DShot300 physical validation;
  it is intentionally rejected by the current API.
- Added an application-owned motor controller with the only production
  `motor_output_t`, mapping, accepted-command timestamp, clock, and timeout.
  No command source can obtain the private output object or backend context.
- Required exact `ARMED` lifecycle, `OK`/`WARNING`/`DEGRADED` health, complete
  command revalidation, and freshness before mapping and forwarding a command.
- Added periodic safety synchronization so producer silence expires the
  retained command. Command acceptance only replaces that complete mapped
  snapshot; it no longer submits a physical frame from producer/USB context.
- Made invalid/stale commands and `UNKNOWN` health enter `FAILSAFE` while
  armed and force stop. Critical motor initialization, output, and
  force-stop failures use catalogue-owned IDs and enter terminal `FAULT`.
- Added health-aware USB arm admission: `UNKNOWN` and `CRITICAL` are rejected
  before the state machine, while warning/degraded health remains armable.
- Added an automated every-build and CTest production-source boundary check
  rejecting raw motor output/DShot calls and new arm-event sites outside their
  explicitly allowed owner files.
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
- Added a production DShot300 adapter that converts normalized physical-order
  motor commands into 18-by-4 compare tables without knowing the V1 pin or
  timer-channel assignment.
- Added the complete V1 PC6-PC9/TIM8/DMA2 Stream 1 Channel 7 register backend.
  All four CCR values move in one update-triggered timer DMA burst, with PWM
  preload, bounded completion/error handling, and GPIO-low rest state.
- Moved physical-output-to-CCR reordering and the persistent active DMA buffer
  entirely into the V1 board layer. The adapter supplies `ESC_M1` through
  `ESC_M4`; the board copies each row into CCR1 through CCR4 order before
  accepting an asynchronous transfer.
- Added a prebuilt application stop table. Force-stop aborts pending output;
  the board copies/reorders that table into its owned DMA storage and sends it
  synchronously.
- Extended the generic motor-output interface with backend status so the
  application detects DMA failures that occur after an accepted submission.
- Registered a highest-priority 1,000 Hz motor-control task for lifecycle,
  health, timeout, asynchronous backend-error synchronization, and periodic
  one-shot retransmission of the latest fresh command.
- Initialized the backend during boot and made an accepted initial zero frame
  mandatory. No USB, receiver, or flight-controller command producer was
  added, so this image has no software path to nonzero throttle.
- Added adapter, board-ordering, generic-status, and asynchronous safety tests
  plus `docs/dshot-motor-backend.md`.
- Added the strict `motor_test` USB request with decimal-to-millionths parsing
  and correlated responses. It accepts logical motors 1 through 4 and the full
  normalized range while each request keeps the other three motors at zero.
- Routed every accepted manual request through `motor_control_submit()` and
  extended the source boundary check so USB cannot bypass lifecycle, health,
  freshness, mapping, backend, force-stop, or fault policy.
- Added reusable host protocol parameters and a motor workflow. It requires a
  separate arm request, sends five seconds of zero frames,
  refreshes the 100 ms command lease every 20 ms for any positive finite duration, then
  sends five zero requests and disarms even after Ctrl-C or a command failure.
- Extended `./ofc` with explicit `device arm`, `device disarm`, and constrained
  `motor run` commands without changing the non-arming smoke workflow.
- The first physical workflow reached the asynchronous zero-frame preparation
  stage, then the board backend reported an error. The safety path rejected the
  later 2% request, forced stop, and entered terminal `FAULT`; health reported
  critical fault ID 14 with the old generic context 4. No nonzero command was
  accepted.
- Temporarily added a first-failure V1 motor snapshot captured before cleanup clears the
  evidence. It distinguishes precise DMA/setup/timeout reasons and retains DMA
  `LISR/CR/FCR/NDTR`, TIM8 `SR/DIER/CR1/CNT`, output state, and saturating
  submission/interrupt/completion/failure counters while reporting the current
  post-cleanup output state separately.
- Propagated the stable board reason through the generic output diagnostic
  callback into motor fault context. The DMA ISR performs no formatting; the
  1 kHz main-context task emits one `FATAL` `MOTOR` log for the latched failure.
- Temporarily added the read-only `motor_diagnostics` USB command and
  `./ofc device motor-diagnostics`, plus a zero-only
  `./ofc motor verify-zero` workflow that must pass before retrying power.
- Separated producer heartbeat rate from physical DShot rate after the first
  4% and 10% motor-1 tests produced only a twitch at roughly 39 USB-triggered
  frames per second. `motor_control_submit()` now atomically retains a complete
  validated and mapped command without touching the backend. The existing
  highest-priority 1 kHz task checks state, health, lease, and prior completion
  before submitting one fresh DShot frame every millisecond.
- Physical retransmission preserves the producer timestamp, so it cannot renew
  the 100 ms safety lease. A preceding frame still busy at the next 1,000 us
  service deadline is treated as a critical stuck-backend failure and forces
  stop; a healthy DShot300 frame completes in approximately 60 us.

No receiver input, sensor access, persistent flight-data logging, or
flight-control behavior has been implemented. Only the development USB path
can request nonzero throttle.

- Replaced normal `DISARMED` GPIO-low behavior with Betaflight-style periodic
  DShot stop traffic. The highest-priority 1 kHz motor task sends a valid
  all-zero frame in `DISARMED`, `FAILSAFE`, and while armed without a fresh
  power command. Terminal faults and backend failures retain the separate
  TIM8/DMA/GPIO hard-stop path.
- Added a five-second stop-frame preparation latch. USB arm requests return
  `motor_not_ready` before it completes, and the motor boundary itself rejects
  nonzero commands if an alternate caller bypasses USB admission. The latch
  survives ordinary disarm/rearm cycles and resets after an emergency stop.
- Temporarily added `hard_stopped`, `stop_frames_streaming`, and
  `arming_preparation_complete` to motor diagnostics. Restored the bench host
  workflow's five-second zero preparation because hardware isolation proved
  one second insufficient while five seconds was accepted by the initial
  SpeedyBee ESC.
- Removed the temporary USB motor-diagnostics command, frame/register
  snapshots, configuration-mask checks, zero-only workflow, unused motor log
  module, and enlarged transport buffers after physical validation. A compact
  board failure reason remains connected to the existing fault context.
- All 25 native host checks and all 37 Python host-tool tests pass. The same 25
  native checks pass under address/undefined-behavior sanitizers, and clangd
  reports no errors in the changed motor-control, command-processor, or JSON
  protocol units. Debug and Release firmware builds pass with warnings as
  errors; Debug uses 64,724 bytes of Flash and 15,344 bytes of RAM, while
  Release uses 43,332 bytes of Flash and 15,344 bytes of RAM.

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
- USB remains an unauthenticated development/bench source. Its manual motor
  path is deliberately narrow but must not be treated as a security boundary;
  physical access and propeller removal remain part of the test procedure.
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
- Latest accepted command storage, timeout enforcement, lifecycle/health
  gating, asynchronous status, a real force-stop backend, and a constrained
  bench producer are implemented. Receiver/flight-control producers remain
  deliberately absent.
- A backend descriptor is copied, but its non-null context object is not; that
  context must have static or otherwise sufficient lifetime. Accepted submission
  likewise promises only an internal copy, not immediate physical application.
- `motor_mapping_configure()` cannot independently inspect application state.
  Its owner must pass `system_disarmed` only from actual
  `SYSTEM_STATE_DISARMED`; the mapping module rejects a false condition but
  cannot detect a dishonest caller. Physical hard-stop is no longer required
  because a logical permutation cannot turn all-zero stop frames into power.
- Logical aircraft positions match the default output order. ESC-stored
  direction is now a persistent, disarmed-only configuration operation rather
  than an ordinary power command. The mixer convention, expected CW/CCW
  directions, and physically observed directions remain to be selected and
  recorded.
- The selected V1 routes, DMA execution, DShot300 ESC acceptance, synchronized
  four-channel operation, and motor positions are physically verified. Exact
  waveform measurements and motor directions remain open.

## Open questions

- Choose an open-source license before declaring the public source licensed for reuse.
- Decide later whether the exact GCC `15.3.1` reproducibility check should become a documented compatible version range; Milestone 0.2 deliberately matches the proven tester toolchain.
- Complete the documented flight-image SWD, timebase, USB, and lifecycle smoke
  tests on the now-available board and ST-Link.
- Measure WS2812 logic margin and interrupt-latency impact before any in-flight
  use. Discrete LED inoperability and active-low microSD detect are confirmed.

## Next step

Review Milestone 3.3, then flash and validate configuration persistence and all
four motor directions without propellers. After recording the required
directions, implement Milestone 3.4's hardware-independent open-loop quad-X
mixer. Receiver motor submission remains prohibited until the later producer
milestone. Physical motor heartbeat-loss timing remains an explicit pre-flight
task; DShot600 remains deferred.

## Milestone 2.2 CRSF and V1 receiver backend import

- Adapted the tester's hardware-independent CRSF parser and decoder. CRC-8/
  DVB-S2 validation precedes publication; packed channel frames produce all 16
  raw 11-bit values, while link-statistics frames are retained diagnostically.
- Added a generic CRSF receiver-source adapter over an injected byte stream. It
  returns at most one channel frame and examines at most 512 bytes per call, so
  malformed or non-channel traffic cannot monopolize the scheduler.
- Resolved the V1 route to normal, non-inverted UART4 at 420000 baud: PC10 TX
  and PC11 RX use alternate function 8, with circular RX on DMA1 Stream 2,
  Channel 4. The 512-byte buffer holds about 12.2 ms of serial traffic. Parsing
  stays outside interrupts; the DMA transfer-complete interrupt counts buffer
  wrap epochs and the UART interrupt retains error handling.
- Registered the receiver service every 1 ms at high priority after the
  highest-priority motor task. It applies the measured normalization profile,
  classifies freshness at 25/100 ms, records bounded diagnostics, and reports
  UART/source failure as a recoverable receiver fault. It cannot submit motor
  commands or trigger `FAILSAFE` in Phase 2.
- Added host tests for parser/decoder correctness, recovery, source ownership,
  valid-frame precedence, stream errors, and the processing bound. The later
  physically proven absolute DMA accounting correction is now synchronized
  before flight-image receiver validation.
- All 30 native tests pass normally and under address/undefined-behavior
  sanitizers. Debug and Release firmware builds pass with warnings as errors.
  The wired receiver image uses 78,328/51,856 bytes of Flash and 16,408 bytes
  of RAM in both Debug and Release builds.

## Milestone 2.4 normalization and freshness

- Added a separately testable normalizer with a validated, copied configuration
  for channel assignment, axis/throttle endpoints, reversal, and the arm-switch
  high threshold. The replaceable RP1 development profile now uses the latest
  successful tester result: roll 174/992/1805, pitch 175/992/1811, throttle
  174/1785, and yaw 355/997/1713 on AETR channels 1-4. Channel 5 remains the
  provisional arm input with a conservative 1500 threshold.
- Added piecewise centered-axis conversion to `[-1, 1]`, throttle conversion to
  `[0, 1]`, clamping, reversal, conservative switch interpretation, and a
  normalized snapshot that preserves the raw reception timestamp and sequence.
- Added a separate stateless freshness evaluator with injected fresh/loss
  boundaries and `UNAVAILABLE`, `FRESH`, `STALE`, and `LOST` outcomes. Clock
  rollback fails closed as `LOST`. The wired development policy treats data as
  stale after 25 ms and lost after 100 ms; these values must be validated before
  receiver data gains control authority.
- Extended the service so one clock sample drives each invocation, a new raw
  and normalized pair is published atomically only after successful conversion,
  and freshness advances even when no new frame arrives. Callers retrieve the
  normalized snapshot and freshness together.
- Added dedicated normalizer and freshness unit tests plus service integration
  coverage. This layer adds no UART, DMA, CRSF parsing, USB schema, lifecycle,
  or motor behavior.
- Documented the replaceable calibration and timestamp policy in
  `docs/receiver-normalization.md`.
- Recorded traceability to tester result
  `20260908T174707Z_002D003E3435471135383539_036e0ee8-81c6-459b-b24e-84487bbbe086.json`.
  Its extrema come only from accepted CRC-valid frames. Its 3,041 CRC and 1,913
  framing errors remain an investigation in the tester session and must be
  synchronized into the imported parser/backend before physical validation.
- All 28 native host checks pass normally and under address/undefined-behavior
  sanitizers, all 37 Python host-tool tests pass, and Debug/Release firmware
  builds passed with warnings as errors before the physical backend import.

## Milestone 2.3 receiver service

- Added `flight/receiver/receiver_source.h` as the portable handoff from the
  future tester-proven UART/CRSF implementation. Each non-blocking call returns
  at most one complete decoded 16-channel frame, no frame, invalid data, or a
  source error.
- Added `app/receiver_service.c`, which owns the latest accepted frame, captures
  its monotonic reception timestamp, assigns a saturating sequence, and tracks
  bounded poll/outcome statistics. Invalid and error results cannot replace or
  refresh the last valid snapshot.
- Added the scheduler-compatible `receiver_service_task()` callback, but did
  not register it in production without a real source or guess its period and
  priority before tester timing evidence exists.
- Added host tests with an injected source and clock. No CRSF constants, UART
  selection, channel normalization, fault policy, lifecycle transition, or
  motor behavior is introduced by this milestone.
- Documented the integration and ownership boundary in
  `docs/receiver-service.md`.
- All 26 native host checks pass normally and under address/undefined-behavior
  sanitizers. Debug and Release firmware builds pass with warnings as errors;
  because the service is deliberately not instantiated yet, linked image usage
  remains 64,724/43,332 bytes of Flash and 15,344 bytes of RAM.

## Milestone 1.11 bring-up evidence

The detailed experiments below are retained as engineering history. Their
temporary USB/register/frame diagnostics have been removed from production;
only compact board failure reasons remain in fault context.

- The normal and address/undefined-behavior sanitizer host builds each run all
  25 native test executables/checks successfully. The Python package runs 34
  tests, including zero-only behavior and one-fragment USB attach recovery.
- Tests prove diagnostic callbacks are mandatory, stable board reason codes
  reach the critical fault context, structured responses contain the complete
  snapshot, the new command is read-only, and zero verification never requests
  nonzero throttle. Board-map coverage now also proves CCR-ordered frame
  reconstruction and rejection of malformed trailing-low slots; USB tests
  prove the maximum-width diagnostics response fits its fixed capacity.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 69,380 bytes of Flash and reserves
  16,296 bytes of RAM; Release uses 47,312 bytes of Flash and reserves 16,296
  bytes of RAM.
- The dirty diagnostic Debug image based on `5748fd0` was programmed,
  read-back verified, and reset with the ESC battery disconnected. It reports
  `DISARMED`, current board output state `IDLE`, no latched reason, and zero
  asynchronous submissions, interrupts, completions, and failures.
- The first diagnostic ESC-powered zero-only retry captured `FEIF1` after 8 of
  72 halfword transfers (`NDTR=64`) with no completion. The ISR stopped TIM8
  and DMA, restored the pins low, and latched `DMA_FIFO_ERROR`; the application
  correctly entered terminal `FAULT`, and no nonzero throttle was requested.
- The captured `FCR=0x80` showed that FIFO-error interrupts were enabled while
  the stream remained in direct mode. The board engine now follows the STM32F4
  TIM DMA-burst configuration pattern: FIFO mode is enabled with a full
  threshold for every transfer, and FIFO-error interrupts are enabled only for
  asynchronous transfers. This correction passed host and firmware builds and
  was flashed with a clean diagnostic baseline.
- The next zero-only attempt completed its first 72-halfword table with one
  completion interrupt and no diagnostic failure. The host nevertheless waited
  for its 512-byte serial read timeout before sending the next heartbeat, so
  the firmware's 100 ms command lease expired and correctly entered failsafe;
  cleanup then disarmed it. The host reader now blocks for one byte and drains
  only bytes reported as already waiting, allowing a newline-complete response
  to return immediately. All 34 Python tests pass.
- The repeated one-second zero-only run completed 38 active refresh requests
  plus five cleanup zero writes. Diagnostics reported 43 submissions, 43
  interrupts, 43 completions, zero failures, no latched reason, and final board
  state `IDLE`; cleanup left the lifecycle `DISARMED`.
- The separately armed first nonzero workflow completed its one-second zero
  preparation, 11 logical-motor-1 refresh requests at 2% over 0.25 seconds,
  five cleanup zeros, and disarm. The cumulative counters advanced from 43 to
  98 with matching submissions, interrupts, and completions, zero failures,
  no latched reason, final board state `IDLE`, and lifecycle `DISARMED`.
  Physical motor motion, identity, and direction await operator confirmation.
- Later 4%/0.75-second and 10%/one-second tests also completed without firmware
  or DMA errors, but the motor only twitched. The host produced approximately
  39 physical frames per second because every frame was still tied to a
  correlated USB request. This directly motivated the 1 kHz retained-command
  output change.
- The 1 kHz image was then flashed and a one-second zero-only run produced
  1,135 matched submissions/completions from 39 USB heartbeats with no failure.
  Repeated motor-1 10% runs produced roughly 2,140 frames each, but still only
  an arming-tone sequence followed by a commutation twitch.
- Updated motor-control tests prove arming without a command remains stopped,
  producer acceptance performs no immediate output, each idle service release
  repeats the retained mapped command, replacement is atomic, retransmission
  preserves the heartbeat timestamp, the inclusive 100 ms lease still
  expires, and a transfer busy for 1,000 us becomes a critical fault even when
  a newer heartbeat arrived meanwhile.
- After motor 1 produced the same arming tones but only a commutation twitch at
  10%, the constrained bench selector was moved to logical motor 2. The host
  and firmware still agree on exactly one permitted motor, with the existing
  10% throttle and one-second duration limits unchanged.
- Added explicit `PREPARING`, `ACTIVE`, `CLEANUP`, and `DISARMED` host events.
  Motor diagnostics now retain the last accepted nonzero physical-order
  normalized command, DShot values and frames, and timer tick profile across
  cleanup so the next physical run can distinguish arming-tone movement from
  the powered phase and verify the runtime encoding.
- Added independent board-boundary diagnostics from the successful nonzero
  transfer. The board reconstructs four frames from its owned table after the
  physical-to-CCR transformation and retains them in CCR1-through-CCR4 order,
  so the selected motor-2 test must report `[0,0,7950,0]`. It also captures a
  ten-bit configuration mask before DMA can consume the table, then records
  that the stream, update request, and timer counter were enabled. A complete
  mask is `1023` and covers DMA addresses/count/channel/word width/FIFO
  mode, TIM8 burst/period/PWM outputs, and GPIO alternate functions.
- After the correctly reordered motor-2 frame still produced only a twitch,
  aligned the V1 TIM8 DMAR path with Betaflight's STM32F4 burst
  representation: the application API remains a compact 16-bit compare table,
  while the board now widens its owned, reordered 72-entry DMA table to
  32-bit words and configures both DMA memory and peripheral widths as words.
  No timing, frame encoding, repetition, safety, or timer-lifecycle behavior
  changed, preserving a single-variable hardware test.
- Aligned the remaining successful-transfer lifecycle with Betaflight's STM32F4
  timer-burst pattern. A normal DMA completion now disables only TIM8 update
  DMA requests and DMA2 Stream 1, leaving TIM8 running and PC6 through PC9 in
  AF3 at the trailing zero compares between 1 kHz frames. Initialization,
  explicit force-stop/disarm, transfer-start failure, and interrupt/DMA error
  paths still stop TIM8, clear all compares, and force every motor pin to
  GPIO-low. Frame timing, encoding, throttle, repetition rate, and the 100 ms
  command lease are unchanged.
- Flashed and physically exercised that lifecycle change with motor 2 at 10%
  for one second. Diagnostics reported 2,138 matched submissions, interrupts,
  and completions, zero failures, physical frame `[0,7950,0,0]`, timer frame
  `[0,0,7950,0]`, and configuration mask `1023`; cleanup stopped TIM8/DMA and
  returned to `DISARMED`. The motor still only twitched, ruling out per-frame
  TIM8 stop/restart as the cause of the observed ESC behavior.
- Prepared a combined follow-up diagnostic at the owner's direction: DShot300
  duty changes from 37.5%/75% to Betaflight-matched 35%/70% (`196`/`392`
  ticks), the host sends five seconds of zero preparation, and selector `0`
  applies the same command to all four motors for up to five active seconds.
  The firmware still enforces the 10% ceiling, 100 ms renewable lease, complete
  zero cleanup, and disarm. Because all three variables change together, a
  successful physical result will require later isolation to identify its
  cause.
- Flashed the combined diagnostic with the ESC battery disconnected, verified
  a clean `DISARMED`/`IDLE` baseline, then ran the explicitly armed all-motors
  10% workflow after battery reconnection. All motors spun during the active
  phase. Post-cleanup diagnostics reported 10,141 matched submissions,
  interrupts, and completions, zero failures, four physical and timer frames
  of `7950`, timing `560/196/392`, configuration mask `1023`, and final
  `DISARMED`/`IDLE` state. This physically validates simultaneous four-channel
  DShot300 output under the combined settings, but does not yet distinguish
  pulse width, five-second zero preparation, or all-motor activation as the
  causal fix.
- Removed only the all-motors diagnostic behavior for the first isolation
  experiment. The firmware and host again accept only logical motor 2, while
  retaining the working 35%/70% pulse widths, five-second zero preparation,
  five-second active limit, 10% ceiling, lease, cleanup, and disarm behavior.
- The motor-2-only isolation then ran for five seconds and the selected motor
  spun continuously. Diagnostics reported 10,138 matched submissions,
  interrupts, and completions, zero failures, physical frame `[0,7950,0,0]`,
  timer frame `[0,0,7950,0]`, timing `560/196/392`, configuration mask `1023`,
  and final `DISARMED`/`IDLE`. This rules out simultaneous all-motor activation
  as necessary for the successful result.
- Reduced only the zero-frame preparation from five seconds to one second for
  the next isolation image. Motor selection, pulse widths, active-duration
  limit, throttle ceiling, lease, cleanup, and disarm remain unchanged.
- The one-second-preparation motor-2 test did not spin, while retaining the
  previously successful `560/196/392` timing and five-second active phase.
  Diagnostics reported 6,144 matched submissions, interrupts, and completions,
  zero failures, physical frame `[0,7950,0,0]`, timer frame `[0,0,7950,0]`,
  configuration mask `1023`, and final `DISARMED`/`IDLE`. Together with the
  successful five-second-preparation motor-2 test, this establishes that the
  longer zero-frame preparation is necessary in the current ESC power-up test
  sequence. Pulse-width necessity remains unisolated.
- Restored five-second zero preparation and reverted only the duty profile to
  the original 37.5%/75% (`210`/`420` ticks) for the next isolation image.
  Motor 2 selection, five-second active duration, 10% throttle ceiling, lease,
  cleanup, and disarm remain unchanged.
- The original-pulse isolation test spun motor 2 continuously after five
  seconds of zero preparation. Diagnostics reported 10,144 matched submissions,
  interrupts, and completions, zero failures, physical frame `[0,7950,0,0]`,
  timer frame `[0,0,7950,0]`, timing `560/210/420`, configuration mask `1023`,
  and final `DISARMED`/`IDLE`. This rules out the 35%/70% pulse change as
  necessary and identifies the longer preparation period as the only tested
  change correlated with reliable spin. A final wait-versus-zero-frame test is
  still needed to distinguish simple ESC boot time from required valid DShot
  zero traffic.
- For the final distinction, the ESC was freshly power-cycled and left
  disarmed for more than five seconds with no DShot traffic, after which the
  unchanged original-pulse firmware ran only one second of zero preparation
  before the five-second motor-2 command. The motor did not spin. Diagnostics
  remained clean with cumulative matched submissions, interrupts, and
  completions, no failures, physical frame `[0,7950,0,0]`, timer frame
  `[0,0,7950,0]`, timing `560/210/420`, configuration mask `1023`, and final
  `DISARMED`/`IDLE`. This proves elapsed ESC boot time alone is insufficient:
  the current ESC requires more than one and no more than five seconds of valid
  zero DShot frames before accepting throttle reliably.
- Returned only the host preparation interval to one second for the final
  boot-time-versus-zero-traffic isolation. The already-flashed firmware remains
  unchanged at original `560/210/420` timing and logical motor 2; no flash is
  required for this host-only change.
- Expanded each fixed USB transmit entry and the command processor's owned
  pending response from 768 to 1,024 bytes so the bounded diagnostics response
  still fits without allocation. This adds fixed RAM only; receive bounds and
  backpressure behavior are unchanged.

## Milestone 1.10 software verification

- Native USB protocol tests cover valid motor/throttle fields, strict decimal
  grammar, malformed or extra fields, integer bounds, and exact accepted and
  rejected response serialization.
- Command-processor tests prove the 10% and selected-motor policies are enforced in
  firmware, only the selected logical command entry is nonzero, all accepted
  traffic uses the motor-control safety gate, and rejection statistics and
  errors are correlated.
- Python tests cover parameter serialization and reserved-envelope protection,
  host-side bounds, no implicit arming, zero-frame preparation, 20 ms active
  refresh, normal cleanup, and Ctrl-C cleanup.
- The normal and address/undefined-behavior sanitizer host builds each run all
  25 native test executables/checks, and all pass. The Python host package runs
  28 tests, and all pass.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 63,368 bytes of Flash and reserves
  15,176 bytes of RAM; Release uses 42,504 bytes of Flash and reserves 15,176
  bytes of RAM.
- No physical verification is claimed. The battery remained disconnected
  during implementation; ESC recognition, motor identity, actual stop latency,
  and disconnect behavior remain Milestone 1.11 checks.

## Milestones 1.8 and 1.9 software verification

- The normal and address/undefined-behavior sanitizer builds each run all 25
  host test executables/checks successfully.
- DShot backend tests prove exact normalized conversion, strict physical
  `M1`-through-`M4` output order, busy behavior, caller-independent copying,
  and fail-closed board result/status mapping.
- Board-map tests prove the reversed physical-to-CCR row transformation,
  including safe in-place use; the board engine owns the only buffer passed to
  DMA.
- Motor-output and motor-control tests cover the new asynchronous status path,
  including conversion of a post-acceptance backend error into a critical
  motor-output fault and forced stop.
- Route tests retain the exact ESC_M1/PC9/CCR4 through
  ESC_M4/PC6/CCR1 arrangement.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 60,020 bytes of Flash and reserves
  15,168 bytes of RAM; Release uses 39,556 bytes of Flash and reserves 15,168
  bytes of RAM.
- clangd reports zero errors for the board engine, application adapter, motor
  control, and composition changes. The Release ELF has no unresolved symbols,
  the source-boundary check passes, and Git whitespace validation passes.
- No physical verification is claimed. Pin state, timer/DMA execution, DShot
  pulse widths, ESC recognition, cancellation latency, motor identity, and
  direction remain propeller-free Milestone 1.11 checks after the constrained
  Milestone 1.10 command path exists.

## Milestone 1.7 verification

- The normal and address/undefined-behavior sanitizer builds each run all 24
  host test executables/checks successfully.
- Motor safety-policy tests cover every health enum value and prove that only
  `OK`, `WARNING`, and `DEGRADED` permit arm/output consideration.
- Motor-control tests cover invalid/backend/initial-stop initialization,
  mandatory stopped startup, private mapping, accepted/busy/error submission,
  command canonicalization, warning/degraded operation, state and health
  rejection, invalid/stale failsafe entry, periodic expiration, critical fault
  reporting, force-stop failure, and stopped-state tracking.
- USB tests prove `UNKNOWN` health rejects arm before state-machine mutation
  with a distinct request-ID-correlated `health_rejected` response.
- An every-build and CTest source scan rejects accidental raw `motor_output`,
  DShot encode/timing, or new arm-event uses outside explicit owner files.
- Debug and Release firmware configurations build with warnings treated as
  errors using Arm GCC 15.3.1. Debug uses 53,476 bytes of Flash and reserves
  14,744 bytes of RAM; Release uses 35,580 bytes of Flash and reserves 14,744
  bytes of RAM.
- The gate uses fixed storage, bounded operations, an injected monotonic clock,
  and main-context-only state/fault access. It adds no allocation, interrupt,
  GPIO, timer, DMA, DShot transmission, USB motor command, or physical output.
- No physical stop is claimed. Backend cancellation, DMA ownership, waveform
  timing, ESC response, and timeout-service latency remain Milestones 1.8-1.11
  and the propeller-free hardware checklist.

## Milestone 1.6 verification

- The normal and address/undefined-behavior sanitizer builds each run all 21
  host test executables successfully.
- DShot timing tests prove the exact 168 MHz DShot300 profile: 560 timer ticks
  per bit, a 196-tick zero high, and a 392-tick one high.
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
  passes, clangd reports zero errors for both motor production modules, Git
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
