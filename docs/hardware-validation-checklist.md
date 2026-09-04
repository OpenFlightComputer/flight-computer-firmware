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
- [ ] Validate PC6-PC9 TIM8 motor routing, channel order, waveform, and DMA on
  hardware during Phase 1; the tester did not validate DShot.

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
