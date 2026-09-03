# Hardware validation checklist

The V1 manufacturing tester has now physically accepted SWD/reset, clocks, USB
with a board workaround, WS2812, BMI270, BMP388, and microSD on a real board.
That evidence is recorded in `docs/v1-bringup-carryover.md`. The checks below
remain flight-firmware checks: tester success, a host test, or a successful
firmware link must not be recorded as proof that the flight image works.

## Basic boot and timebase

- [ ] Program Debug and Release images over SWD and verify reset reaches
  `BOOT_STATUS_RUNNING`.
- [ ] Confirm HSE/PLL startup and the 168 MHz core clock on hardware.
- [ ] Confirm the embedded dirty-aware build identity matches the flashed image.
- [ ] Measure the TIM5 counter rate as 1 MHz against external time.
- [ ] Exercise or accelerate a TIM5 32-bit wrap and confirm monotonic 64-bit
  time across the interrupt boundary.

## USB enumeration and protocol

- [ ] Verify the development VID/PID, descriptors, V1-disabled VBUS sensing,
  and CDC device enumeration on the target hosts.
- [ ] Verify status, health, and log JSON contain exact unsigned 64-bit decimal
  values without relying on target-library `%llu` support.
- [ ] Send fragmented, coalesced, LF, and CRLF commands and verify exactly one
  response with the matching request ID.
- [ ] Verify malformed input returns `request_id:null` and a valid unsupported
  command echoes its numeric request ID.
- [ ] Interleave commands and logs and confirm the host can demultiplex them by
  `type` without assuming that a response is the next physical line.
- [ ] Disconnect and reconnect during idle, receive, queued transmit, and active
  transmit; confirm bounded recovery without a stuck queue.

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

- [ ] Confirm successful startup reports `DISARMED` and health `OK`.
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
- [ ] Measure WS2812 3.3 V-to-5 V logic margin and the scheduling impact of its
  roughly 30 microsecond interrupt-masked update before allowing in-flight use.
- [ ] Validate PC6-PC9 TIM8 motor routing, channel order, waveform, and DMA on
  hardware during Phase 1; the tester did not validate DShot.
