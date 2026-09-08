# Receiver normalization and freshness

The receiver service converts each complete decoded 16-channel frame into an
owned raw snapshot and an owned normalized control snapshot in one operation.
The conversion is hardware- and protocol-independent; UART, DMA, CRSF framing,
and CRC validation stay behind `receiver_source_t`.

## Initial calibration profile

The compiled RP1 development profile comes from the latest successful tester
run completed at `2026-09-08T17:48:06Z` for MCU UID
`002D003E3435471135383539` and test UUID
`036e0ee8-81c6-459b-b24e-84487bbbe086`. The result file is
`20260908T174707Z_002D003E3435471135383539_036e0ee8-81c6-459b-b24e-84487bbbe086.json`.

| Control | CRSF channel | Minimum | Center | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Roll | 1 | 174 | 992 | 1805 |
| Pitch | 2 | 175 | 992 | 1811 |
| Throttle | 3 | 174 | — | 1785 |
| Yaw | 4 | 355 | 997 | 1713 |

The tester persisted extrema from every accepted CRC-valid channel frame. Axis
centers are inferred from the final sample, whose roll/pitch/yaw values were
992/992/997. The profile maps channel 5 to the arm indication but retains the
conservative raw threshold of 1500 because the test did not assign or qualify a
specific arm switch.

All values live in `receiver_normalization_config_t`; the normalizer copies a
validated configuration during initialization. A future setup workflow can
therefore supply measured endpoints, centers, assignments, and reversals to the
same algorithm without rewriting it. Configurations require five distinct
in-range channels, strictly ordered axis endpoints, and valid throttle bounds.

Centered axes use separate denominators below and above center, clamp to
`[-1, 1]`, and optionally reverse sign. Throttle maps its endpoints to
`[0, 1]`, clamps, and can reverse. The switch comparison is conservative:
values below the configured high threshold are not an arm-high indication.
Normalization reports the switch level only and has no lifecycle authority.

## Timestamp and atomic publication

The service takes one monotonic `time_us()` sample per invocation. When the
source returns a frame, the service normalizes into temporary storage first. It
publishes the raw and normalized snapshots only if normalization succeeds. Both
snapshots receive the same reception timestamp and saturating source sequence.
Invalid frames, source errors, and normalization errors preserve the previous
snapshots and cannot renew freshness.

## Freshness

Freshness uses injected configuration rather than guessed production values.
The fresh boundary must be positive and strictly earlier than the loss
boundary. For a valid snapshot:

```text
age <= fresh_through_us             FRESH
fresh_through_us < age <= lost_after_us  STALE
age > lost_after_us                 LOST
```

No snapshot produces `UNAVAILABLE`. A clock value earlier than the reception
timestamp fails closed as `LOST`. The service reevaluates freshness every task
invocation even when no new frame arrives, while the normalized values and
their original timestamp remain available for diagnostics.

The combined `receiver_control_state_t` accessor returns normalized values and
freshness together. Future consumers must require `FRESH`; Phase 2 gives the
receiver no motor or lifecycle authority.

Host tests cover configuration rejection and copying, the evidence-derived
defaults, asymmetric
axis scaling, clamping, reversal, switch threshold, metadata preservation,
fresh/stale/lost boundaries, clock rollback, service integration, atomic
replacement, and saturation. Exact endpoints, channel assignments, timeouts,
and physical packet age remain tester/board validation items. The same run
reported 8,252 accepted frames, 3,041 CRC errors, and 1,913 framing errors; the
discarded frames did not contribute extrema, but that error rate must be
resolved before importing the UART/CRSF backend into operational firmware.
