# Receiver service boundary

Milestone 2.3 introduced the application side of receiver processing. The
physically exercised tester implementation is now adapted behind that boundary
and the service is registered in the production task registry.

## Integration seam

```text
V1 UART4 circular DMA + portable CRSF parser
                         |
                         v
                 receiver_source_t
                         |
             one decoded frame per call
                         |
                         v
                 receiver_service_t
              /          |          \
             v           v           v
      raw snapshot  normalization  freshness
                         |           |
                         +-----+-----+
                               v
                    combined control state
                               |
                               v
                  receiver-loss policy decision
                  (observed only during Phase 2)
```

`receiver_source_t` is a non-blocking injected callback. One invocation returns
no frame, one complete decoded 16-channel frame, an invalid-frame indication,
or a source error. The source writes into caller-owned storage and may not keep
the pointer. UART buffering, CRSF framing, CRC checks, and protocol recovery
remain below this interface.

The service calls the source exactly once per invocation. A valid frame is
copied into service-owned storage, timestamped with the injected monotonic
clock, and assigned a saturating sequence number. No-frame, invalid-frame, and
source-error results update saturating statistics without modifying the latest
valid snapshot. The service normalizes a new frame before atomically publishing
the raw and control snapshots, then reevaluates freshness on every invocation.

## Scheduling

`receiver_service_task()` remains the generic scheduler callback. Production
wraps it with bounded diagnostics and runs it every 1 ms at high priority,
below the highest-priority 1 kHz motor-output task. At 420000 baud, about 42
wire bytes arrive per millisecond; the circular DMA buffer holds 512 bytes and
the CRSF adapter processes at most 512 bytes per invocation. The task normally
returns after the first decoded channel frame. Its arming interlock may request
lifecycle changes, but it cannot submit a motor command.

The V1 DMA reader uses an absolute consumer count and an absolute producer
count formed from the current `NDTR` position plus a wrap epoch advanced by the
DMA transfer-complete interrupt. It samples the volatile epoch around `NDTR`
with memory barriers and retries if the epoch changes. This removes the
full-wrap ambiguity of equal modulo positions. If producer minus consumer
exceeds the 512-byte capacity, the reader counts one overrun and the exact
number of overwritten bytes, advances to the oldest retained byte, and
continues parsing. Overrun and dropped-byte totals are debugger-visible beside
the UART and parser diagnostics. The receiver task is independent of the
lower-priority USB/logging service, so output backpressure cannot decide when
receiver input is drained.

The separate receiver-loss policy classifies the latest snapshot into live,
hold, Stage 1 fallback, or latched Stage 2 stop actions. The application logs
transitions and reports a recoverable connection-loss fault. Phase 3.2 now
allows only a live, fresh, startup-low-qualified arm-switch edge to request
receiver ownership, and a fresh low switch to disarm that receiver-owned
session. Receiver control values still cannot reach motor output. See
`receiver-normalization.md`, `receiver-failsafe.md`, and
`receiver-arming.md`.

## On-demand USB inspection

The `receiver` USB command calls a read-only inspection provider in application
context. The provider copies the raw and normalized snapshots already owned by
the receiver service, calculates packet age from the monotonic clock, and adds
the current failsafe, link, UART, parser, and DMA diagnostics. It does not
consume DMA bytes or call the receiver task. Consequently, inspection is idle
when no host requests it, and even a slow terminal display cannot delay the
high-priority receiver input path.

`./ofc device receiver --watch` polls that command at 10 Hz by default and
recreates the tester's raw-channel position and session minimum/maximum view,
while also showing the flight firmware's normalized controls. The host owns
the display-only extrema; firmware publishes no continuously maintained
inspection snapshot.

The Phase 2 Release flight image was flashed to Flight Computer V1 and the
live view displayed the connected RP1's raw and normalized controls,
freshness, failsafe state, and diagnostics. This closes the Phase 2 physical
receiver-inspection boundary. Phase 3.2 subsequently added receiver arm/disarm
authority while retaining the prohibition on receiver motor-command
submission.

Host tests use a fake source and clock to prove dependency validation,
single-call boundedness, caller-storage independence, timestamp and sequence
replacement, preservation across invalid/error results, task-callback behavior,
normalization, freshness transitions, and counter saturation. Dedicated
policy tests cover exact timeout boundaries, requested controls, invalid
configuration, short-loss recovery, Stage 2 latching and recovery, unsafe
recovery interruption, unavailable input, and clock rollback. Parser/source
tests additionally cover CRC, packed-channel decoding, recovery, link
statistics, malformed frames, stream errors, and the bounded byte budget.
