# Receiver-loss policy

The receiver-loss policy converts the age of the latest normalized receiver
snapshot into a requested control action. It is independent of CRSF, UART,
the scheduler, lifecycle transitions, and motor output. During Phase 2 the
application only observes and logs this decision; it does not submit it to the
motor-control path.

## Default stages

| Latest frame age | Policy state | Requested action |
| --- | --- | --- |
| 0–25 ms | `LIVE` | Use the current normalized controls |
| over 25–100 ms | `STALE_HOLD` | Hold the latest controls |
| over 100–400 ms | `LOSS_HOLD` | Continue holding the latest controls and report receiver loss |
| over 400–1500 ms | `STAGE_ONE` | Neutral roll/pitch/yaw and 5% throttle |
| over 1500 ms | `STAGE_TWO_LATCHED` | Request stopped output and require deliberate recovery |

No valid frame at startup produces no requested control. Receiver loss is
reported after 100 ms, and the Stage 2 stop request latches after 1500 ms.
These rules avoid inventing a command when there is no last valid input.

A fresh frame automatically leaves either hold state or Stage 1. Stage 2 does
not automatically restore live control. It continues requesting stop until
fresh frames show both the arm switch low and throttle at or below 5% for a
continuous 500 ms. The policy then marks recovery as ready, but a future
Phase 3 owner must explicitly acknowledge recovery after returning the
lifecycle to `DISARMED`. Any stale, armed, or high-throttle input resets the
recovery timer. A backwards clock observation fails closed into Stage 2.

## Configuration

`receiver_failsafe_config_t` owns every timing and fallback value. The current
defaults are compiled into `receiver_failsafe_default_config()` and are
validated before use. Timing thresholds must be strictly ordered, the
recovery duration must be nonzero, axes must be in `[-1, 1]`, and throttles
must be in `[0, 1]`; NaN values are rejected.

The structure deliberately uses scalar fields that map directly to a future
JSON configuration object, for example:

```json
{
  "stale_after_us": 25000,
  "loss_detected_after_us": 100000,
  "hold_last_until_us": 400000,
  "stage_two_after_us": 1500000,
  "recovery_stable_us": 500000,
  "stage_one_roll": 0.0,
  "stage_one_pitch": 0.0,
  "stage_one_yaw": 0.0,
  "stage_one_throttle": 0.05,
  "recovery_throttle_maximum": 0.05
}
```

JSON transport and persistent storage are intentionally deferred. When those
are added, decoded values must pass the same validator before replacing the
active configuration; invalid or incomplete configuration must never partly
apply.

The 5% Stage 1 throttle is a deterministic development value, not a measured
hover or climb setting. It cannot guarantee altitude gain, and neutral rate
commands cannot level an already tilted vehicle. The roadmap therefore keeps
its replacement with vehicle-specific, stabilized recovery as explicit work.

## Diagnostics and faults

The application exposes the current policy state, requested action, frame
age, transition count, latch flag, recovery-ready flag, and complete decision
as debugger-visible variables. It logs only state transitions. Receiver loss
after 100 ms reports the recoverable `FAULT_ID_RECEIVER_CONNECTION_LOST`,
which degrades health without changing lifecycle state. The fault clears on
automatic recovery before Stage 2; a Stage 2 latch retains it until explicit
recovery is implemented.
