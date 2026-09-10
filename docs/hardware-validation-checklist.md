# Hardware validation checklist

The V1 manufacturing tester has now physically accepted SWD/reset, clocks, USB
with a board workaround, WS2812, BMI270, BMP388, and microSD on a real board.
That evidence is recorded in `docs/v1-bringup-carryover.md`. The checks below
remain flight-firmware checks: tester success, a host test, or a successful
firmware link must not be recorded as proof that the flight image works.

## Basic boot and timebase

- [x] Program the Debug image over SWD, verify it, reset, and confirm
  `BOOT_STATUS_RUNNING`.
- [ ] Program and smoke-test the Release image over SWD.
- [x] Confirm HSE/PLL startup and the 168 MHz core clock on the Debug image.
- [ ] Use `./ofc smoke` to confirm the embedded dirty-aware build identity
  matches the freshly built and flashed Release image.
- [x] Measure the TIM5-derived uptime against host monotonic time; a 2.009334 s
  host interval produced 2.006966 s of firmware time (ratio 0.99882).
- [ ] Exercise or accelerate a TIM5 32-bit wrap and confirm monotonic 64-bit
  time across the interrupt boundary.

## USB enumeration and protocol

- [x] Verify development identity `CAFE:4002`, the flight-firmware product
  descriptor, V1-disabled VBUS sensing, and CDC enumeration on macOS.
- [x] Verify ordinary status uptime and log timestamp/sequence fields are valid
  target-generated JSON integers without target-library `%llu` support.
- [ ] Force `UINT64_MAX` through a target status/log/health output path and
  inspect its exact decimal bytes.
- [x] Send fragmented, coalesced, LF, and CRLF commands and verify exactly one
  response with the matching request ID.
- [x] Verify malformed input returns `request_id:null` and recovers normally.
- [x] Verify a valid unsupported command echoes its numeric request ID.
- [x] Interleave commands and logs and confirm the host can demultiplex them by
  `type` without assuming that a response is the next physical line.
- [x] Close and reopen the host serial connection and confirm uptime advances
  without an MCU reset.
- [ ] Physically disconnect and reconnect during idle, receive, queued transmit,
  and active transmit; confirm bounded recovery without a stuck queue.

## USB overload and interrupt/main ownership

- [ ] Burst input faster than the 64-byte-per-millisecond service budget and
  verify raw-byte, completed-line, and oversized-line drop counters.
- [ ] Force a raw-ring overflow in the middle of a command and prove no suffix
  or truncated fragment executes; the next complete newline-delimited command
  must recover normally.
- [ ] Fill the two completed-line slots and verify later lines are counted and
  discarded without corrupting retained lines.
- [ ] Fill transmit and logging queues while disconnected, then reconnect and
  verify response retention, ordering, and resumed log draining.
- [ ] Stress receive and transmit completion concurrently while inspecting all
  shared indices, flags, queue counts, and saturation counters.

## Execution time and stack

- [ ] Measure `usb-service` last/max duration for malformed maximum-length JSON,
  status, arm/disarm, worst-case health, and worst-case escaped logging.
- [ ] Confirm no scheduler overrun or unacceptable delay to higher-priority
  ready tasks under sustained USB traffic.
- [ ] Instrument stack high-water usage on the MCU, including nano-printf
  internals, interrupts, worst-case health serialization, manual 64-bit decimal
  conversion, and log formatting.
- [ ] Confirm the reserved minimum stack and actual RAM separation retain an
  evidence-based safety margin.

## Fault, health, and lifecycle

- [x] Confirm successful startup reports `DISARMED`, health `OK`, zero active
  faults, and zero dropped fault records.
- [ ] Inject each feasible non-critical USB failure and confirm startup remains
  `DISARMED` with `DEGRADED` health.
- [ ] Inject representative critical startup failures and confirm the fault
  record is published before terminal `FAULT` and halt.
- [ ] Confirm an ordinary runtime fault does not automatically leave `ARMED`.
- [ ] Confirm fault-registry capacity exhaustion increments the dropped counter,
  enters `FAULT`, and reports `CRITICAL` rather than continuing normally with
  incomplete diagnostics.

## BMI270 flight-firmware path

- [ ] Flash the Milestone 4.1 image and confirm
  `firmware_imu_initialization_result` and
  `firmware_imu_initial_sample_result` are both zero.
- [ ] Inspect all six `firmware_imu_raw_*` values while stationary, then reboot
  in several known orientations and confirm the one-shot raw samples respond
  without an active `FAULT_ID_IMU_INITIALIZATION` record. Continuous rotation
  inspection belongs to the scheduled sampling milestone.
- [ ] Confirm SPI3 mode 0, the 656.25 kHz clock, PB3/PB4/PB5 AF6 routing, and
  active-low PD2 chip select on the flight image. Tester evidence does not
  replace this flight-image validation.

## Manufacturing evidence to preserve

- [x] D4/D5 discrete LEDs were confirmed inoperable on V1. They must not carry
  boot, health, arm, or fault meaning.
- [x] PC5 microSD card detect was confirmed active-low.
- [x] PA1 WS2812 GRB/MSB-first output worked on the first board using DWT-timed
  GPIO.
- [ ] Confirm the flight image drives PA1 low without an enable glitch, emits
  one all-zero startup frame, and leaves the RGB LED off after both a cold
  power cycle and an MCU-only reset.
- [ ] Measure WS2812 3.3 V-to-5 V logic margin and the scheduling impact of its
  roughly 30 microsecond interrupt-masked update before allowing in-flight use.
- [ ] Confirm PC6-PC9 begin GPIO-low during cold boot and MCU reset; remain AF3
  with zero compares between normal stop frames; and return GPIO-low after an
  emergency force-stop or injected DMA failure.
- [ ] Validate PC6-PC9 TIM8 motor routing, CCR1/M4 through CCR4/M1 channel
  order, DShot300 pulse widths, synchronized edges, and DMA completion/error
  behavior; the tester did not validate DShot.
- [ ] Measure the synchronous zero-frame force-stop duration and confirm its
  interrupt masking does not lose USB service or TIM5 overflow state.
- [ ] Confirm the 1 kHz motor-control task detects an injected asynchronous DMA
  error and command expiry within the documented scheduler bound.
- [x] With propellers removed, validate DShot300 first against the SpeedyBee
  BLS 60A 30x30 4-in-1 ESC (stock BLHeli_S J-H-40): begin with only
  PC9/ESC_M1 nonzero through the constrained host command, confirm ESC
  recognition, identify S1-S4/motor order one at a time, then exercise multiple
  synchronized nonzero channels. Keep the telemetry request clear because the
  stock ESC has no separate telemetry output.
- [x] With the diagnostic image and ESC battery disconnected, confirm
  `motor_diagnostics` starts invalid with zero asynchronous counters. After
  reconnecting the battery, run the zero-only workflow before any nonzero
  command and preserve the diagnostic response before resetting.
- [x] After the selected motor-2 nonzero test, confirm the retained physical
  frames are `[0,7950,0,0]`, the post-reordering timer frames are
  `[0,0,7950,0]` in CCR1-through-CCR4 order, and the runtime configuration
  flags equal the expected mask `1023`.
- [x] For the combined all-motors diagnostic, confirm five seconds of zero
  preparation precede five seconds at 10%, all four retained physical and
  timer frames equal `7950`, timing reports `560/196/392`, configuration flags
  equal `1023`, all counters match, and cleanup returns to `DISARMED`.
- [x] Repeat with only logical motor 2 while retaining five-second preparation
  and `560/196/392` timing, to isolate whether simultaneous motor activation
  contributed to the successful run.
- [x] Repeat logical motor 2 with one-second preparation while retaining
  `560/196/392` timing and the five-second active phase, to isolate whether the
  extended zero preparation contributed to the successful run.
- [x] Restore five-second preparation and original `560/210/420` timing while
  retaining logical motor 2 and the five-second active phase, to isolate whether
  narrower pulse widths contributed to the successful run.
- [x] Distinguish ESC boot time from zero-frame conditioning: after a fresh ESC
  power cycle, wait five seconds without arming or DShot traffic, then use only
  one second of zero preparation before the motor-2 test.
- [x] Flash the continuous-stop-stream image with the ESC battery disconnected.
  After five seconds in `DISARMED`, confirm diagnostics report
  `hard_stopped=false`, `stop_frames_streaming=true`,
  `arming_preparation_complete=true`, matching submission/completion counts,
  and no failure.
- [x] Reconnect the ESC battery while the USB-powered FC remains `DISARMED`,
  wait five seconds, then arm and run logical motor 2 at 10%. Confirm continuous
  rotation, automatic zero/disarm cleanup, and no new diagnostic failure.
- [ ] Physically validate loss of the 100 ms producer heartbeat and record the
  actual motor-stop latency. This is deferred by owner decision while receiver
  development begins.
- [x] Implement a persistent, disarmed-only runtime direction configuration
  without requiring firmware recompilation or wiring changes.
- [ ] Confirm persistence across a power cycle and a normal firmware flash,
  then record every motor's rotation direction before first flight.

## Flight-image evidence log

### 2026-09-03 — Debug image at commit `5db525a`

- ST-Link `B55B5A1A00000000F1EBF501`, firmware V2J46S7, target voltage 3.26 V.
- STM32F405 device ID `0x413`, revision Y; program/read-back verification and
  hardware reset succeeded at approximately 950 kHz SWD.
- USB enumerated at `/dev/cu.usbmodem101` as `CAFE:4002`, manufacturer
  `OpenFlightComputer`, product `OpenFlightComputer Flight Firmware`.
- SWD RAM/register inspection found boot status `3`, USB initialization result
  `0`, `SystemCoreClock=168000000`, TIM5 enabled, update interrupt enabled,
  `PSC=83`, and `ARR=0xffffffff`.
- At the same snapshot, fast/medium/slow task counts were 105740/10574/1058 and
  USB service count was 105740, matching the intended 1000/100/10 Hz ratios.
- Correlated status and health responses reported `DISARMED`, `OK`, zero active
  faults, complete fault data, and zero dropped records. Fragmented, CRLF,
  coalesced, malformed/unsupported-command handling, log/response interleaving,
  and host close/reopen checks passed.

### 2026-09-05 — First DShot bench attempt at commit `5748fd0`

- Props were removed and motors secured. USB and SWD were connected; the ESC
  was battery powered for the test.
- USB arm was accepted. During the motor workflow's zero-frame preparation,
  the asynchronous backend entered error. The following request was rejected
  by state, cleanup ran, and no 2% command was accepted.
- Status reported terminal `FAULT`. Health reported one complete critical
  application fault: ID 14 (`MOTOR_OUTPUT`), occurrence one, context 4. That
  context identified only generic backend status, motivating the latched
  pre-cleanup diagnostic snapshot before repetition.

### 2026-09-05 — Diagnostic zero-frame attempt based on commit `5748fd0`

- The diagnostic build started `DISARMED` with the board output `IDLE`, no
  latched reason, and all asynchronous counters at zero.
- After arming, the zero-only workflow submitted one 72-halfword table. DMA2
  Stream 1 asserted `FEIF1` after consuming 8 halfwords (`NDTR=64`) and before
  any completion. The ISR stopped the transfer, drove PC6-PC9 low, and the
  application entered terminal `FAULT`; no nonzero command was requested.
- The retained `FCR=0x80` exposed FIFO-error interrupts enabled while direct
  mode was selected. The subsequent software correction enables FIFO mode with
  a full threshold, matching the bundled STM32F4 TIM DMA-burst configuration
  pattern.
- After flashing that correction, one complete 72-halfword zero table finished
  with one DMA completion interrupt and no failure. The host did not refresh
  the 100 ms command lease in time because its 512-byte serial read waited for
  the 100 ms read slice after receiving the short response. Firmware entered
  failsafe and host cleanup disarmed it as designed. The host read path was
  corrected.
- The repeated one-second zero-only test then completed 38 requested refreshes
  plus five cleanup writes. The retained counters reported 43 submissions, 43
  interrupts, 43 completions, and zero failures; no diagnostic reason was
  latched, the output returned to `IDLE`, and cleanup left the system
  `DISARMED`.
- The first separately authorized nonzero workflow completed 11 logical-motor-1
  requests at 2% over 0.25 seconds after its zero preparation, then sent five
  cleanup zeros and disarmed. Cumulative diagnostics reached 98 submissions,
  interrupts, and completions with zero failures and no latched reason.
  Operator confirmation of which motor moved and in which direction remains
  pending.
- Follow-up logical-motor-1 tests at 4% for 0.75 seconds and 10% for one second
  also completed with matching DMA submission/completion counts and no fault,
  but produced only a small physical twitch. The one-second active phase sent
  39 USB-triggered physical frames, showing that command/response timing was
  incorrectly controlling the DShot repetition rate.
- The follow-up software change retains each accepted producer command and
  makes the existing highest-priority 1 kHz motor task submit the next one-shot
  frame after checking state, health, 100 ms lease, and prior completion. It
  requires a new battery-disconnected flash, zero-only run, and physical motor
  retry before the twitch cause is considered resolved.
