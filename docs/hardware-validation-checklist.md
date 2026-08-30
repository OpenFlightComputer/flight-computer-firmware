# Hardware validation checklist

These checks are intentionally pending until a Flight Computer V1 and required
debug hardware are available. A host test or successful firmware link must not
be recorded as physical evidence for any item below.

## Basic boot and timebase

- [ ] Program Debug and Release images over SWD and verify reset reaches
  `BOOT_STATUS_RUNNING`.
- [ ] Confirm HSE/PLL startup and the 168 MHz core clock on hardware.
- [ ] Measure the TIM5 counter rate as 1 MHz against external time.
- [ ] Exercise or accelerate a TIM5 32-bit wrap and confirm monotonic 64-bit
  time across the interrupt boundary.

## USB enumeration and protocol

- [ ] Verify the development VID/PID, descriptors, VBUS sensing, and CDC device
  enumeration on the target hosts.
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
  internals, interrupts, worst-case health serialization, and log formatting.
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

## Deferred board observations

- [ ] Confirm the documented discrete LED polarity/connectivity.
- [ ] Confirm WS2812 logic-level margin.
- [ ] Confirm microSD card-detect polarity when the storage milestone begins.
