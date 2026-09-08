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
returns after the first decoded channel frame and cannot control motors.

## Deliberately deferred

USB serialization, connection/loss fault policy, lifecycle transitions, and
motor authority remain deferred. In particular, receiver loss cannot enter
`FAILSAFE` during Phase 2 because the receiver does not control motors until
Phase 3. See `receiver-normalization.md`.

Host tests use a fake source and clock to prove dependency validation,
single-call boundedness, caller-storage independence, timestamp and sequence
replacement, preservation across invalid/error results, task-callback behavior,
normalization, freshness transitions, and counter saturation. Parser/source
tests additionally cover CRC, packed-channel decoding, recovery, link
statistics, malformed frames, stream errors, and the bounded byte budget.
