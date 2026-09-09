# Receiver arming interlock

Milestone 3.2 connects the normalized receiver arm switch to the existing
motor-control lifecycle boundary. It does not submit receiver throttle or any
other motor command.

## Input and ownership

The existing 1 kHz receiver task runs the CRSF source, normalization,
freshness, and receiver-loss policy before calling the arming interlock. The
interlock accepts input only when the normalized snapshot is valid and fresh
and the failsafe decision is `LIVE` with action `LIVE`. Unavailable, stale,
lost, fallback, and recovery input clears the previous low-switch
qualification.

The normalized `arm_switch_high` value comes from the configured receiver
switch channel and raw high threshold. It is only an input request. The
authoritative armed state remains in `system_state_machine_t`, and the
authoritative owner remains in `motor_control`.

## Safe edge behavior

At startup, or after input becomes unusable or another source owns control,
the receiver must first be observed with its arm switch low. A later fresh
low-to-high edge can request `motor_control_arm(RECEIVER)` only when normalized
throttle is at or below the shared `MOTOR_COMMAND_STOP_THRESHOLD` (`0.001`).
The configuration validator cannot raise the arming limit above that canonical
stop threshold.

A high switch at startup cannot arm. If a rising edge is rejected because
throttle is above the limit or common motor-control admission fails, lowering
throttle while leaving the switch high cannot arm. The operator must move the
switch low and then high again.

When the receiver owns motor control, a fresh low switch requests the common
disarm path. A receiver switch cannot arm or disarm a `USB_TEST`-owned session.
After any disarm, ownership returns to `NONE`, and either source can acquire it
through its own explicit safe arm sequence.

## Deliberate milestone boundary

The interlock only changes lifecycle and authority. No receiver control value
is converted into a motor command in this milestone, so an accepted receiver
arm continues to produce stop frames. The later mixer and receiver-control
producer milestones add powered output through the same source-aware motor
gate.

Host tests cover initialization, startup-high rejection, one-shot low-to-high
arming, throttle rejection and required retoggle, stale/failsafe reset,
receiver disarming, USB isolation, and rejected lifecycle operations. Physical
validation can inspect `state`, `control_source`, the receiver switch, and the
debugger-visible interlock result without connecting the flight battery.
