# Motor control authority

Milestone 3.1 adds one source latch to the existing lifecycle and motor-control
safety boundary. It is deliberately not another state machine. The lifecycle
still decides whether the vehicle is `DISARMED`, `ARMED`, `FAILSAFE`, or in
`FAULT`; authority answers only which producer may replace the retained motor
command while the lifecycle is `ARMED`.

## Sources and invariant

The sources are `NONE`, `USB_TEST`, and `RECEIVER`. A successful arm request
atomically transitions the lifecycle and records its source:

```text
DISARMED + successful USB arm      -> ARMED + USB_TEST
DISARMED + successful receiver arm -> ARMED + RECEIVER
disarm, failsafe, or force-stop    -> non-armed + NONE
```

Only the active source may call `motor_control_submit()`. A command carrying
another source returns `MOTOR_CONTROL_SUBMIT_BLOCKED_SOURCE` without replacing
or invalidating the current owner's command. This is important because the
future 1 kHz receiver producer would otherwise overwrite a slower USB bench
command, while a delayed USB packet could briefly overwrite receiver output.

Within the selected source, the most recently accepted complete motor command
still wins. Lifecycle, stop-frame preparation, health, validity, freshness,
mapping, and backend checks remain unchanged.

## Arm and release ownership

`motor_control_arm()` is now the only production owner of
`SYSTEM_STATE_EVENT_ARM_REQUESTED`. It validates the source, requires
`DISARMED`, applies health admission, requires completed stop-frame
preparation, performs the lifecycle transition, and only then latches the
source. A failed arm never assigns authority.

`motor_control_disarm()` accepts the existing legal lifecycle disarm paths and
immediately invalidates the retained command and clears authority. Failsafe
entry, any successful force-stop, and even a failed hardware force-stop also
clear software authority. The periodic synchronization path removes authority
whenever it observes a non-armed lifecycle state, covering external critical
fault transitions as well.

Safety actions are not source-restricted. Disarm, failsafe, critical faults,
and emergency stopping can always remove authority and stop output. The source
restriction applies only to powered command submission.

## Debugging and future receiver use

The `status` USB response includes `control_source`, and
`motor_control_active_source()` exposes the same bounded value to application
diagnostics. `NONE` is reported whenever the lifecycle is not `ARMED`, so the
observable combinations stay simple.

Milestone 3.1 wires USB arm and motor-test commands to `USB_TEST`. Milestone
3.2 uses the `RECEIVER` source only after a fresh, startup-low-qualified,
low-throttle switch edge. It can release receiver ownership through the common
disarm path, but it still cannot submit motor commands. See
`receiver-arming.md`.
