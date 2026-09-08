# Receiver service boundary

Milestone 2.3 introduces the application side of receiver processing without
duplicating the tester's CRSF or UART implementation. It is deliberately not
wired into the production task registry until a physically proven receiver
source is imported.

## Integration seam

```text
tester-proven UART + CRSF parser (future import)
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

`receiver_service_task()` is the scheduler-compatible callback. Production does
not register it yet because there is no flight-firmware receiver source. Once
the tester-proven implementation is imported, its period and priority will be
selected from the observed receiver frame rate, UART buffering depth, measured
execution time, and DMA/interrupt design rather than guessed now.

## Deliberately deferred

The initial channel assignments and conventional CRSF calibration are a
replaceable development profile, not physical calibration evidence. Production
freshness timeouts, USB serialization, fault severity, lifecycle transitions,
and motor authority remain deferred. In particular, receiver loss cannot enter
`FAILSAFE` during Phase 2 because the receiver does not control motors until
Phase 3. See `receiver-normalization.md`.

Host tests use a fake source and clock to prove dependency validation,
single-call boundedness, caller-storage independence, timestamp and sequence
replacement, preservation across invalid/error results, task-callback behavior,
normalization, freshness transitions, and counter saturation. Physical UART and
CRSF behavior remains tester and board evidence.
